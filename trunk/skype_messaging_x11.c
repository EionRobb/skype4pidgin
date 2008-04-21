#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Display *disp = NULL;
static Window win = (Window)-1;
Window skype_win = (Window)-1;
static GThread *receiving_thread = NULL;
Atom message_start, message_continue;
static gboolean run_loop = FALSE;
static unsigned char x11_error_code = 0;

static void receive_message_loop(void);
int x11_error_handler(Display *disp, XErrorEvent *error);

static gboolean
skype_connect()
{
	Window root;
	Atom skype_inst;
	Atom type_ret;
	int format_ret;
	unsigned long nitems_ret;
	unsigned long bytes_after_ret;
	unsigned char *prop;
	int status;
	
	XSetErrorHandler(x11_error_handler);
	disp = XOpenDisplay(getenv("DISPLAY"));
	if (disp == NULL)
	{
		skype_debug_info("skype", "Couldn't open display\n");
		return FALSE;
	}
	message_start = XInternAtom( disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False );
	message_continue = XInternAtom( disp, "SKYPECONTROLAPI_MESSAGE", False );
	root = DefaultRootWindow( disp );
	win = XCreateSimpleWindow( disp, root, 0, 0, 1, 1,
		0, BlackPixel( disp, DefaultScreen( disp ) ),
		BlackPixel( disp, DefaultScreen( disp ) ));
	XFlush(disp);
	if (win == -1)
	{
		skype_debug_info("skype", "Could not create X11 messaging window\n");
		return FALSE;
	}
	skype_inst = XInternAtom(disp, "_SKYPE_INSTANCE", True);
	if (skype_inst == None)
	{
		skype_win = (Window)-1;
		skype_debug_info("skype_x11", "Could not create skype Atom\n");
		return FALSE;
	}
	status = XGetWindowProperty(disp, root, skype_inst, 0, 1, False, XA_WINDOW, &type_ret, &format_ret, &nitems_ret, &bytes_after_ret, &prop);
	if(status != Success || format_ret != 32 || nitems_ret < 1)
	{
		skype_win = (Window)-1;
		skype_debug_info("skype", "Skype instance not found\n");
		return FALSE;
	}
	skype_win = * (const unsigned long *) prop & 0xffffffff;
	
	run_loop = TRUE;
	
	if (receiving_thread == NULL)
		receiving_thread = g_thread_create((GThreadFunc)receive_message_loop, NULL, FALSE, NULL);
	
	return TRUE;
}


static void
skype_disconnect()
{
	XEvent *e;
	
	run_loop = FALSE;
	skype_win = (Window)-1;
	
	e = g_new0(XEvent, 1);
	e->xclient.type = DestroyNotify;
	XSendEvent(disp, win, False, 0, e);
	
	//wait here for the event to be handled
	
	
	XDestroyWindow(disp, win);
	XCloseDisplay(disp);
	
	win = (Window)-1;
	disp = NULL;
}

int
x11_error_handler(Display *disp, XErrorEvent *error)
{
	x11_error_code = error->error_code;
	return FALSE;
}


static void
send_message(char* message)
{
	unsigned int pos = 0;
	unsigned int len = strlen( message );
	XEvent e;
	int message_num;
	char error_return[15];
	unsigned int i;
	
	if (skype_win == -1 || win == -1 || disp == NULL)
	{
		//There was an error
		if (message[0] == '#')
		{
			//And we're expecting a response
			sscanf(message, "#%d ", &message_num);
			sprintf(error_return, "#%d ERROR", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)g_strdup(error_return), FALSE, NULL);
		}
		return;
	}
	
	memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.message_type = message_start; /* first message */
	e.xclient.display = disp;
	e.xclient.window = win;
	e.xclient.format = 8; /* 8-bit values */

	do
	{
		for( i = 0; i < 20 && i + pos <= len; ++i )
			e.xclient.data.b[ i ] = message[ i + pos ];
		XSendEvent( disp, skype_win, False, 0, &e );

		e.xclient.message_type = message_continue; /* 2nd or greater message */
		pos += i;
	} while( pos <= len );
	
	//XFlush(disp);
	
	if (x11_error_code == BadWindow)
	{
		//There was an error
		if (message[0] == '#')
		{
			//And we're expecting a response
			sscanf(message, "#%d ", &message_num);
			sprintf(error_return, "#%d ERROR", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)g_strdup(error_return), FALSE, NULL);
		}
		g_thread_create((GThreadFunc)skype_message_received, "CONNSTATUS LOGGEDOUT", FALSE, NULL); 
		return;
	}
}

static void
receive_message_loop(void)
{
	XEvent e;
	GString *msg = NULL;
	char msg_temp[21];
	size_t len;
	Bool event_bool;
	
	msg_temp[20] = '\0';
	while(run_loop)
	{
		//XNextEvent(disp, &e);
		//if (e.type != ClientMessage)
		//{
		//	skype_debug_info("skype_x11", "Unknown event received: %d\n", e.xclient.type);
		//	XFlush(disp);
		//	continue;
		//}
		if (!disp)
		{
			skype_debug_error("skype_x11", "display has disappeared\n");
			break;
		}
		event_bool = XCheckTypedEvent(disp, ClientMessage, &e);
		if (!event_bool)
		{
			usleep(1000);
			continue;
		}
		
		strncpy(msg_temp, e.xclient.data.b, 20);
		len = strlen(msg_temp);
		if (e.xclient.message_type == message_start)
			msg = g_string_new_len(msg_temp, len);
		else if (e.xclient.message_type == message_continue)
			msg = g_string_append_len(msg, msg_temp, len);
		else
		{
			skype_debug_info("skype_x11", "unknown message type: %d\n", e.xclient.message_type);
			XFlush(disp);
			continue;
		}

		if (len < 20)
		{
			//if (msg->str[0] == '#')
				g_thread_create((GThreadFunc)skype_message_received, (void *)g_string_free(msg, FALSE), FALSE, NULL);
			//else
			//	purple_timeout_add(1, (GSourceFunc)skype_handle_received_message, (gpointer)g_string_free(msg, FALSE));
			XFlush(disp);
		}
	}
	receiving_thread = NULL;
}

static void
hide_skype()
{
	
}

static gboolean
exec_skype()
{
	GError *error;
	if (g_spawn_command_line_async("skype", &error))
	{
		return TRUE;
	} else {
		skype_debug_error("skype", "Could not start skype: %s\n", error->message);
		return FALSE;
	}
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
}

