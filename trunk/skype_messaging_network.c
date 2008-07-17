#include <glib.h>
#include <errno.h>
#include <string.h>
//#include <sys/ioctl.h>

#ifdef _WIN32
#define fsync(fd) _commit(fd)
#endif

static guint input_timeout;
static gint source_sock = -1;
static gboolean connected = FALSE;
static gboolean in_progress = FALSE;
static gchar etb_string[2] = {23, 0};


void
read_function_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	int len;
	gchar response[3096];
	static GString *response_string = NULL;
	gchar *reply;
	gchar **reply_pieces;
	int i;

	len = read(source, response, sizeof(response)-1);
	if (len < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			skype_disconnect();
		}
		return;
	}
	
	if (len == sizeof(response)-1)
	{
		if (response_string == NULL)
			response_string = g_string_new_len(response, len);
		else
			response_string = g_string_append_len(response_string, response, len);
	}
	else
	{
		if (response_string)
		{
			response_string = g_string_append_len(response_string, response, len);
			reply = g_string_free(response_string, FALSE);
		}
		else if (len)
			reply = g_strndup(response, len);
		else
			return;
		reply = g_strstrip(reply);
		reply_pieces = g_strsplit(reply, etb_string, -1);
		g_free(reply);
		i = 0;
		for (i=0; reply_pieces[i]; i++)
		{
			reply = reply_pieces[i];
			if (g_str_equal(reply, "LOGIN"))
			{
				connected = TRUE;
				in_progress = FALSE;
			} else {
				g_thread_create((GThreadFunc)skype_message_received, reply, FALSE, NULL);
			}
		}
		response_string = NULL;
	}
}

gpointer
skype_read_thread(gpointer data)
{
	while(connected || in_progress)
	{
		read_function_cb(data, source_sock, PURPLE_INPUT_READ);
		g_thread_yield();
		sleep(1);
	}
	return data;
}

void
connect_function(gpointer data, gint source, const gchar *error_message)
{
	gchar *loginmsg;
	PurpleAccount *acct = skype_get_account(NULL);
	
	if (error_message)
	{
		in_progress = FALSE;
		g_thread_create((GThreadFunc)skype_message_received, "CONNSTATUS LOGGEDOUT", FALSE, NULL);
		return;
	}
	source_sock = source;
	
	loginmsg = g_strdup_printf("LOGIN %s %s", acct->username, acct->password);
	send_message(loginmsg);
	g_free(loginmsg);
	
	g_thread_create((GThreadFunc)skype_read_thread, NULL, FALSE, NULL);
}

static gboolean
skype_connect()
{
	return connected;
}

static void
skype_disconnect()
{
	if (!connected)
		return;
	
	send_message("QUIT");

	connected = FALSE;
	close(source_sock);
	source_sock = -1;
	purple_input_remove(input_timeout);
	in_progress = FALSE;
}

static void
send_message(char* message)
{
	int message_num;
	char *error_return;
	char *temp;
	int msglen = -1;
	int len = 0;
	
	if (message)
	{
		msglen = strlen(message) + 1;
		temp = g_strdup_printf("%s%c", message, 23);
		len = write(source_sock, temp, msglen);
		fsync(source_sock);
	}
	
	if (len != msglen)
	{
		//There was an error
		if (message[0] == '#')
		{
			//And we're expecting a response
			sscanf(message, "#%d ", &message_num);
			error_return = g_strdup_printf("#%d ERROR NETWORK", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)error_return, FALSE, NULL);
		}
	}
}

static void
hide_skype()
{
	//don't need to since SILENT_MODE ON works
	return;
}

static gboolean
exec_skype()
{
	if (!connected && !in_progress)
	{
		in_progress = TRUE;
		PurpleAccount *acct = skype_get_account(NULL);
		purple_proxy_connect(acct->gc, acct, "5.5.242.9", 5000, connect_function, acct);
		g_thread_create((GThreadFunc)skype_read_thread, acct, FALSE, NULL);
	}
	return TRUE;
}

static gboolean
is_skype_running()
{
	return connected || in_progress;
}
