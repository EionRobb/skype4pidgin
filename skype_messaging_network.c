#include <glib.h>

static PurpleProxyConnectData *proxy_data = NULL;
static guint input_timeout;
static gint source_sock;

void
read_function_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	int len;
	gchar response[3096];
	static GString response_string = NULL;

	len = read(source, response, sizeof(response)-1);
	if (len < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			close(source);
			purple_input_remove(input_timeout);
		}
		return;
	}
	if (len != 0)
	{
		if (response_string == NULL)
			response_string = g_string_new_len(response, len);
	}
	if (len == 0)
	{
		
		g_thread_create((GThreadFunc)skype_message_received, g_strdup(response), FALSE, NULL);
	}
}

void
connect_function(gpointer data, gint source, const gchar *error_message)
{
	if (error_message)
	{
		g_thread_create((GThreadFunc)skype_message_received, "CONNSTATUS LOGGEDOUT", FALSE, NULL);
		return;
	}
	source_sock = source;
	input_timeout = purple_input_add(source, PURPLE_INPUT_READ, read_function_cb, data);
	
}

static gboolean
skype_connect()
{
	PurpleAccount acct = skype_get_account(NULL);

	proxy_data = purple_proxy_connect(acct->gc, acct, "delilah", 5000, connect_function, acct);

	return TRUE;
}


static void
skype_disconnect()
{
	
}

static void
send_message(char* message)
{
	int message_num;
	char *error_return;

	//There was an error
	if (message[0] == '#')
	{
		//And we're expecting a response
		sscanf(message, "#%d ", &message_num);
		error_return = g_strdup_printf("#%d ERROR NETWORK", message_num);
		g_thread_create((GThreadFunc)skype_message_received, (void *)error_return, FALSE, NULL);
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
	return TRUE;
}

static gboolean
is_skype_running()
{
	return TRUE;
}
