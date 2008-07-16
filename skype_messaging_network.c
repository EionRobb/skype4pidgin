#include <glib.h>
#include <errno.h>
#include <string.h>
//#include <sys/ioctl.h>

#ifdef _WIN32
#define fsync(fd) _commit(fd)
#endif

static PurpleProxyConnectData *proxy_data = NULL;
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
	//if (len == 0 && response_string && response_string->str)
	//else if (len == 0)
	//{
	//	skype_disconnect();
	//}
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
			//printf("skype Received %d %s\n", len, reply);
			if (g_str_equal(reply, "LOGIN"))
			{
				connected = TRUE;
				in_progress = FALSE;
			} else {
				g_thread_create((GThreadFunc)skype_message_received, reply, FALSE, NULL);
			}
		}
		//g_strfreev(reply_pieces);
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
/*
void
read_function_thread(gpointer source_pointer)
{
	gint source = GPOINTER_TO_INT(source_pointer);
	fd_set fds;
	struct timeval tv;
	gint select_return;
	
	FD_ZERO(&fds);
	FD_SET(source, &fds);
	
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	
	//int nonblock = 0;

	//turn off non blocking mode
	//setsockopt(source, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock));
	//ioctl(source, FIONBIO, &nonblock);
	while (connected || in_progress)
	{
		select_return = select(1, &fds, NULL, NULL, &tv);
		printf("select_return: %d\n", select_return);
		read_function_cb(NULL, source, PURPLE_INPUT_READ);
		g_thread_yield();
		sleep(1);
	}
}*/

void
connect_function(gpointer data, gint source, const gchar *error_message)
{
	gchar *loginmsg;
	PurpleAccount *acct = skype_get_account(NULL);
	
	//printf("skype connect_function\n");
	
	if (error_message)
	{
		g_thread_create((GThreadFunc)skype_message_received, "CONNSTATUS LOGGEDOUT", FALSE, NULL);
		return;
	}
	source_sock = source;
	
	loginmsg = g_strdup_printf("LOGIN %s %s", acct->username, acct->password);
	send_message(loginmsg);
	g_free(loginmsg);
	
	//input_timeout = purple_input_add(source, PURPLE_INPUT_READ, read_function_cb, data);
	//g_thread_create((GThreadFunc)read_function_thread, GINT_TO_POINTER(source), FALSE, NULL);
	g_thread_create((GThreadFunc)skype_read_thread, NULL, FALSE, NULL);
}

gpointer
skype_connect_thread(gpointer data)
{
	PurpleAccount *acct = data;
	proxy_data = purple_proxy_connect(acct->gc, acct, "127.0.0.1", 5000, connect_function, acct);
	//printf("skype connect thread\n");
	return proxy_data;
}

static gboolean
skype_connect()
{
	/*long timeout = 0;
	
	PurpleAccount *acct = skype_get_account(NULL);
	g_thread_create((GThreadFunc)skype_connect_thread, acct, FALSE, NULL);
	//purple_proxy_connect(acct->gc, acct, "127.0.0.1", 5000, connect_function, acct);

	connected = FALSE;
	
	while (!connected && timeout < G_USEC_PER_SEC*10)
	{
		g_thread_yield();
		timeout += 1000;
		usleep(1000);
	}
	
	purple_proxy_connect_cancel(proxy_data);
	*/
	return connected;
}

static void
skype_disconnect()
{
	if (!connected)
		return;
	
	send_message("QUIT");

	connected = FALSE;
	//printf("skype skype_disconnect\n");
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
	int msglen;
	int len;
	
	msglen = strlen(message);
	len = write(source_sock, message, msglen);
	if (len)
	{
		write(source_sock, etb_string, 1);
	}
	fsync(source_sock);
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
		//g_thread_create((GThreadFunc)skype_connect_thread, acct, FALSE, NULL);
		purple_proxy_connect(acct->gc, acct, "127.0.0.1", 5000, connect_function, acct);
		g_thread_create((GThreadFunc)skype_read_thread, acct, FALSE, NULL);
	}
	return TRUE;
}

static gboolean
is_skype_running()
{
	return connected || in_progress;
}
