#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus-glib-lowlevel.h>

#define SKYPE_DBUS_BUS DBUS_BUS_SESSION

static DBusGConnection *connection = NULL;
static DBusGProxy *proxy = NULL;

static DBusHandlerResult
skype_notify_handler(DBusConnection *connection, DBusMessage *message, gpointer user_data)
{
	DBusMessageIter iterator;
	gchar *message_temp;
	DBusMessage *temp_message;
	
	temp_message = dbus_message_ref(message);
	dbus_message_iter_init(temp_message, &iterator);
	if (dbus_message_iter_get_arg_type(&iterator) != DBUS_TYPE_STRING)
	{
		return FALSE;
	}
	
	do
	{
		dbus_message_iter_get_basic(&iterator, &message_temp);
		g_thread_create((GThreadFunc)skype_message_received, g_strdup(message_temp), FALSE, NULL);
	} while(dbus_message_iter_has_next(&iterator) && dbus_message_iter_next(&iterator));
	
	dbus_message_unref(message);
	
	return DBUS_HANDLER_RESULT_HANDLED;
}

static gboolean
skype_connect()
{
	GError *error = NULL;
	DBusObjectPathVTable vtable;
	
	if (connection == NULL)
	{
		connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
		if (connection == NULL && error != NULL)
		{
			skype_debug_info("skype_dbus", "Error: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}
	}
    
    if (proxy == NULL)
    {
		proxy = dbus_g_proxy_new_for_name (connection,
	                                     "com.Skype.API",
	                                     "/com/Skype",
	                                     "com.Skype.API");
	    if (proxy == NULL)
	    	return FALSE;
	    
	    vtable.message_function = &skype_notify_handler;
		dbus_connection_register_object_path(dbus_g_connection_get_connection(connection), "/com/Skype/Client", &vtable, NULL);
    }
    
	return TRUE;
}

static void
skype_disconnect()
{
	/*if (connection != NULL)
		g_free(connection);
	if (proxy != NULL)
		g_free(proxy);
	if (proxy_receive != NULL)
		g_free(proxy_receive);*/
}

static void
send_message(char* message)
{
	GError *error = NULL;
	gchar *str = NULL;
	int message_num;
	gchar error_return[30];
	
	if (!dbus_g_proxy_call (proxy, "Invoke", &error, G_TYPE_STRING, message, G_TYPE_INVALID,
                          G_TYPE_STRING, &str, G_TYPE_INVALID))
	{
    	if (error && error->message)
    	{
	    	skype_debug_info("skype_dbus", "Error sending message: %s\n", error->message);
	    	if (message[0] == '#')
	    	{
	    		//We're expecting a response
				sscanf(message, "#%d ", &message_num);
				sprintf(error_return, "#%d ERROR", message_num);
				g_thread_create((GThreadFunc)skype_message_received, (void *)g_strdup(error_return), FALSE, NULL);
	    	}
	    }
	    else
	    	skype_debug_info("skype_dbus", "no response\n");
	}
	if (str != NULL)
		g_thread_create((GThreadFunc)skype_message_received, (void *)str, FALSE, NULL);
}

static gboolean
is_skype_running()
{
	const gchar *temp;
	int pid;
	gchar* stat_path;
	FILE *fh;
	gchar exec_name[15];
	struct stat *statobj = g_new(struct stat, 1);
	//open /proc
	GDir *procdir = g_dir_open("/proc", 0, NULL);
	//go through directories that are numbers
	while((temp = g_dir_read_name(procdir)))
	{
		pid = atoi(temp);
		if (!pid)
			continue;
		// /proc/{pid}/stat contains lots of juicy info
		stat_path = g_strdup_printf("/proc/%d/stat", pid);
		fh = fopen(stat_path, "r");
		pid = fscanf(fh, "%*d (%15[^)]", exec_name);
		fclose(fh);
		if (!g_str_equal(exec_name, "skype"))
		{
			g_free(stat_path);
			continue;
		}
		//get uid/owner of stat file by using fstat()
		g_stat(stat_path, statobj);
		g_free(stat_path);
		//compare uid/owner of stat file (in g_stat->st_uid) to getuid();
		if (statobj->st_uid == getuid())
		{
			//this copy of skype was started by us
			g_dir_close(procdir);
			g_free(statobj);
			return TRUE;
		}
	}
	g_dir_close(procdir);
	g_free(statobj);
	return FALSE;
} static void
hide_skype()
{
	
}

static gboolean
exec_skype()
{
	GError *error;
	if (g_spawn_command_line_async("skype --enable-dbus --use-session-dbus", &error))
	{
		return TRUE;
	} else {
		skype_debug_error("skype", "Could not start skype: %s\n", error->message);
		return FALSE;
	}
}

