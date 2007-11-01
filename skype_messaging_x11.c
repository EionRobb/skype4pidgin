#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Display *disp = NULL;
static Window win = (Window)-1;
Window skype_win = (Window)-1;
GThread *receiving_thread;
Atom message_start, message_continue;
static gboolean run_loop = TRUE;

static void receive_message_loop(void);

static gboolean skype_connect()
{
	Window root;
	Atom skype_inst;
	Atom type_ret;
	int format_ret;
	unsigned long nitems_ret;
	unsigned long bytes_after_ret;
	unsigned char *prop;
	int status;
	
	/*purple_debug_info("skype_x11", "Starting X11 Connection\n");*/
	
	disp = XOpenDisplay(getenv("DISPLAY"));
	message_start = XInternAtom( disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False );
	message_continue = XInternAtom( disp, "SKYPECONTROLAPI_MESSAGE", False );
	root = DefaultRootWindow( disp );
	win = XCreateSimpleWindow( disp, root, 0, 0, 1, 1,
		0, BlackPixel( disp, DefaultScreen( disp ) ),
		BlackPixel( disp, DefaultScreen( disp ) ));
	XFlush(disp);
	skype_inst = XInternAtom(disp, "_SKYPE_INSTANCE", True);
	status = XGetWindowProperty(disp, root, skype_inst, 0, 1, False, XA_WINDOW, &type_ret, &format_ret, &nitems_ret, &bytes_after_ret, &prop);
	if(status != Success || format_ret != 32 || nitems_ret != 1)
	{
		skype_win = (Window)-1;
		purple_debug_info("skype", "Skype instance not found\n");
		return FALSE;
	}
	skype_win = * (const unsigned long *) prop & 0xffffffff;
	
	run_loop = TRUE;
	
	receiving_thread = g_thread_create((GThreadFunc)receive_message_loop, NULL, FALSE, NULL);
	/*purple_debug_info("skype_x11", "Receiving_thread: %d\n", receiving_thread);*/
	
	return TRUE;
}


static void skype_disconnect()
{
	XEvent e;
	
	run_loop = FALSE;
	skype_win = (Window)-1;
	
	//clear the XEvent buffer
	memset(&e, 0, sizeof(e));
	e.xclient.type = DestroyNotify;
	XSendEvent(disp, win, False, 0, &e);
	XDestroyWindow(disp, win);
	XCloseDisplay(disp);
}


static void send_message(char* message)
{
	unsigned int pos = 0;
	unsigned int len = strlen( message );
	XEvent e;
	
	if (skype_win == -1)
		return;

	memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.message_type = message_start; /* first message */
	e.xclient.display = disp;
	e.xclient.window = win;
	e.xclient.format = 8;

	do
	{
		unsigned int i;
		for( i = 0; i < 20 && i + pos <= len; ++i )
			e.xclient.data.b[ i ] = message[ i + pos ];
		XSendEvent( disp, skype_win, False, 0, &e );

		e.xclient.message_type = message_continue; /* 2nd or greater message */
		pos += i;
	} while( pos <= len );

	/*purple_debug_info("skype_x11", "Flushing\n");*/
	XFlush(disp);
}

static void receive_message_loop(void)
{
	XEvent e;
	GString *msg = NULL;
	char msg_temp[21];
	int last_len, real_len;
	/* TODO Detect XWindow disconnects */
	msg_temp[21] = '\0';
	while(run_loop)
	{
		/* purple_debug_info("skype_x11", "Waiting for event on display %d window %d\n", disp, win); */
		XNextEvent(disp, &e);
		/*purple_debug_info("skype_x11", "Event received\n");*/
		if (e.type != ClientMessage)
		{
			purple_debug_info("skype_x11", "Unknown event received: %d\n", e.xclient.type);
			XPutBackEvent(disp, &e);
			XFlush(disp);
			continue;
		}
		/*purple_debug_info("skype_x11", "Event format %d\n", e.xclient.format);*/
		strncpy(msg_temp, e.xclient.data.b, 20);
		real_len = last_len = strlen(msg_temp);
		if (last_len >= 21)
		{
			last_len = 21;
			real_len = 20;
		}
		/* purple_debug_info("skype_x11", "Received: %20s Length: %d\n", msg_temp, last_len); */
		/*purple_debug_info("skype_x11", "Message type: %s\n", XGetAtomName(disp, e.xclient.message_type));*/
		if (e.xclient.message_type == message_start)
			msg = g_string_new_len(msg_temp, real_len);
		else if (e.xclient.message_type == message_continue)
			msg = g_string_append_len(msg, msg_temp, real_len);
		else
		{	
			XPutBackEvent(disp, &e);
			XFlush(disp);
			continue;
		}

		if (last_len < 21) /* msg->str[msg->len] == '\0') */
		{
			/*purple_debug_info("skype_x11", "Complete message received: %s.  Sending to Skype\n", msg->str);*/
			g_thread_create((GThreadFunc)skype_message_received, (void *)g_strdup(msg->str), FALSE, NULL);
			XFlush(disp);
			usleep(500);
		}
	}
}
