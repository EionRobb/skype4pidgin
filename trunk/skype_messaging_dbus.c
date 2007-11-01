#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus-glib.h>

#define SKYPE_DBUS_BUS DBUS_BUS_SESSION

static DBusGConnection *connection = NULL;
static DBusGProxy *proxy = NULL;
static DBusGProxy *proxy_receive = NULL;

static void notify_handler(DBusGProxy *object, const char *message, gpointer user_data)
{
	purple_debug_info("skype_dbus", "notify_handler called: %s\n", message);
}

static gboolean skype_connect()
{
	GError *error = NULL;
	connection = dbus_g_bus_get (SKYPE_DBUS_BUS, &error);
	if (connection == NULL && error != NULL)
	{
		purple_debug_info("skype_dbus", "Error: %s\n", error->message);
		g_error_free(error);
		return FALSE;
	}
    
	proxy = dbus_g_proxy_new_for_name (connection,
                                     "com.Skype.API",
                                     "/com/Skype",
                                     "com.Skype.API");
	proxy_receive = dbus_g_proxy_new_for_name (connection,
                                     "com.Skype.API",
                                     "/com/Skype/Client",
                                     "com.Skype.API");
    
    dbus_g_proxy_add_signal(proxy_receive, "Notify", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(proxy_receive, "Notify", G_CALLBACK(notify_handler), NULL, NULL);
    
	return TRUE;
}

static void skype_disconnect()
{
	g_free(connection);
	g_free(proxy);
}

static void send_message(char* message)
{
	GError *error = NULL;
	gchar *str = NULL;
	purple_debug_info("skype_dbus", "con %d, proxy %d, rec %d\n", connection, proxy, proxy_receive);
	if (!dbus_g_proxy_call (proxy, "Invoke", &error, G_TYPE_STRING, message, G_TYPE_INVALID,
                          G_TYPE_STRING, &str, G_TYPE_INVALID))
	{
    	if (error && error->message)
	    	purple_debug_info("skype_dbus", "Error: %s\n", error->message);
	    else
	    	purple_debug_info("skype_dbus", "no response\n");
	}
	if (str != NULL)
		g_thread_create((GThreadFunc)skype_message_received, (void *)str, FALSE, NULL);
}

