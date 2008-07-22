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
			if (len > 0)
				response_string = g_string_append_len(response_string, response, len);
			reply = g_string_free(response_string, FALSE);
			response_string = NULL;
		}
		else if (len)
			reply = g_strndup(response, len);
		else
			return;
		reply = g_strstrip(reply);
		reply_pieces = g_strsplit(reply, etb_string, -1);
		g_free(reply);
		for (i=0; reply_pieces[i+1]; i++)
		{
			reply = reply_pieces[i];
			if (g_str_equal(reply, "LOGIN"))
			{
				connected = TRUE;
				in_progress = FALSE;
				skype_debug_info("skype", "Received: LOGIN\n");
			} else {
				g_thread_create((GThreadFunc)skype_message_received, g_strdup(reply), FALSE, NULL);
			}
		}
		//check that we received part of a message
		if (strlen(reply_pieces[i]))
		{
			skype_debug_info("skype", "Last piece: '%s'\n", reply_pieces[i]);
			response_string = g_string_new(reply_pieces[i]);
		}
		g_strfreev(reply_pieces);
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
		g_thread_create((GThreadFunc)skype_message_received, g_strdup("CONNSTATUS LOGGEDOUT"), FALSE, NULL);
		return;
	}
	source_sock = source;
	
	loginmsg = g_strdup_printf("LOGIN %s %s", acct->username, acct->password);
	send_message(loginmsg);
	skype_debug_info("skype", "Sending: 'LOGIN {username} {password}'\n");
	//send_message frees this
	//g_free(loginmsg);
	
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
	
	send_message(g_strdup("QUIT"));

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
		g_free(temp);
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
	
	g_free(message);
}

static void
hide_skype()
{
	//don't need to since SILENT_MODE ON works
	return;
}

gboolean
connection_timeout(gpointer data)
{
	in_progress = FALSE;
	return FALSE;
}

static gboolean
exec_skype()
{
	if (!connected && !in_progress)
	{
		in_progress = TRUE;
		PurpleAccount *acct = skype_get_account(NULL);
		purple_proxy_connect(acct->gc, acct, purple_account_get_string(acct, "host", "skype.robbmob.com"), purple_account_get_int(acct, "port", 5000), connect_function, acct);
		g_thread_create((GThreadFunc)skype_read_thread, acct, FALSE, NULL);
		purple_timeout_add_seconds(10, connection_timeout, acct);
	}
	return TRUE;
}

static gboolean
is_skype_running()
{
	return connected || in_progress;
}
