#include <glib.h>

#include "skype_events.c"

static gboolean skype_handle_received_message(char *message);

static void skype_message_received(char *message);
static gboolean skype_connect();
static void skype_disconnect();
static void send_message(char* message);
static void hide_skype();
static gboolean exec_skype();

void skype_send_message_nowait(char *message, ...);
char *skype_send_message(char *message, ...);

// Sort through the mess of different OS's to get the right proto

#ifdef _WIN32
#	include "skype_messaging_win32.c"
#else /*if !win32 */
#	ifdef __APPLE__
#		include "skype_messaging_carbon.c"
#	else /*if !apple */
#		ifndef SKYPE_DBUS
#			include "skype_messaging_x11.c"
#		else 
#			include "skype_messaging_dbus.c"
#		endif /* !x11 */
#	endif /* !apple */
#endif /* win32 */


typedef struct {
	gpointer sender;
	gpointer body;
	int time;
	gpointer chatname;
} SkypeMessage;

static GHashTable *message_queue = NULL;
static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
static GCond *condition = NULL;

#ifdef _WIN32
//these two #defines override g_static_mutex_lock and
// g_static_mutex_unlock so as to remove "strict-aliasing"
// compiler warnings
#define g_static_mutex_lock2(mutex) \
    g_mutex_lock (g_static_mutex_get_mutex ((GMutex **)(void*)mutex))
#define g_static_mutex_unlock2(mutex) \
    g_mutex_unlock (g_static_mutex_get_mutex ((GMutex **)(void*)mutex))
#else
#define g_static_mutex_lock2 g_static_mutex_lock
#define g_static_mutex_unlock2 g_static_mutex_unlock
#endif

static void
skype_message_received(char *orig_message)
{
	guint request_number;
	guint *key;
	int string_pos;
	char *message;
	
	if (strlen(orig_message) == 0)
		return;
	
	message = g_strdup(orig_message);

	skype_debug_info("skype", "Received: %s\n", message);

	if(message[0] == '#')
	{
		//It's a reply from a call we've made - update the hash table
		sscanf(message, "#%u %n", &request_number, &string_pos);
		key = g_new(guint, 1);
		*key = request_number;
		
		g_static_mutex_lock2(&mutex);
		g_hash_table_insert(message_queue, key, g_strdup(&message[string_pos]));
		g_cond_broadcast(condition);
		g_static_mutex_unlock2(&mutex);
		
		g_free(message);
	} else {
		purple_timeout_add(1, (GSourceFunc)skype_handle_received_message, (gpointer)message);
	}
}

void 
skype_send_message_nowait(char *message_format, ...)
{
	va_list args;
	char* message;
	
	va_start(args, message_format);
	message = g_strdup_vprintf(message_format, args);
	va_end(args);
	
	skype_debug_info("skype", "Sending: '%s'\n", message);
	g_thread_create((GThreadFunc)send_message, message, FALSE, NULL);

}

char *skype_send_message(char *message_format, ...)
{
	static guint next_message_num = 0;
	guint cur_message_num;
	char *message;
	char *return_msg;
	va_list args;
#ifdef __APPLE__
	guint timeout = 0;
#else
	gboolean condition_result;
	GTimeVal endtime = {0,0};
#endif
	
	va_start(args, message_format);
	message = g_strdup_vprintf(message_format, args);
	va_end(args);

	if (!message_queue)
		message_queue = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	
	g_static_mutex_lock2(&mutex);
	if (!condition)
		condition = g_cond_new();
	cur_message_num = next_message_num++;
	if (next_message_num == G_MAXUINT)
		next_message_num = 0;
	g_static_mutex_unlock2(&mutex);
	
	//Send message asynchronously
	skype_send_message_nowait("#%u %s", cur_message_num, message);
	g_free(message);

	g_static_mutex_lock2(&mutex);
	//Wait for a response
	while(g_hash_table_lookup(message_queue, &cur_message_num) == NULL)
	{
		g_static_mutex_unlock2(&mutex);
		g_thread_yield();
			
#ifdef __APPLE__
		RunCurrentEventLoop(0);
		usleep(1000);
		g_static_mutex_lock2(&mutex);

		if (timeout++ == 10000)
#else
		
		//wait for message for a maximum of 10 seconds
		g_get_current_time(&endtime);
		g_time_val_add(&endtime, 10 * G_USEC_PER_SEC);
		condition_result = g_cond_timed_wait(condition, g_static_mutex_get_mutex(&mutex), &endtime);
		
		//g_cond_timed_wait already locks this mutex
		//g_static_mutex_lock2(&mutex);
		
		if(!condition_result)
#endif
		{
			//we timed out while waiting
			g_hash_table_remove(message_queue, &cur_message_num);
			g_static_mutex_unlock2(&mutex);
			return g_strdup("");	
		}
	}
	return_msg = (char *)g_hash_table_lookup(message_queue, &cur_message_num);
	g_hash_table_remove(message_queue, &cur_message_num);
	g_static_mutex_unlock2(&mutex);
	
	if (strncmp(return_msg, "ERROR", 5) == 0)
	{
		g_free(return_msg);
		return g_strdup("");
	}
	return return_msg;
}
