#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Display *disp = NULL;
static Window win = (Window)-1;
Window skype_win = (Window)-1;
GThread *receiving_thread;
Atom message_start, message_continue;
static gboolean run_loop = TRUE;
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
	
	disp = XOpenDisplay(getenv("DISPLAY"));
	if (disp == NULL)
	{
		purple_debug_info("skype", "Couldn't open display\n");
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
		purple_debug_info("skype", "Could not create X11 messaging window\n");
		return FALSE;
	}
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

	XSetErrorHandler(x11_error_handler);
	do
	{
		for( i = 0; i < 20 && i + pos <= len; ++i )
			e.xclient.data.b[ i ] = message[ i + pos ];
		XSendEvent( disp, skype_win, False, 0, &e );

		e.xclient.message_type = message_continue; /* 2nd or greater message */
		pos += i;
	} while( pos <= len );

	XFlush(disp);
	
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
	
	msg_temp[20] = '\0';
	while(run_loop)
	{
		XNextEvent(disp, &e);
		if (e.type != ClientMessage)
		{
			purple_debug_info("skype_x11", "Unknown event received: %d\n", e.xclient.type);
			XFlush(disp);
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
			XFlush(disp);
			continue;
		}

		if (len < 20)
		{
			g_thread_create((GThreadFunc)skype_message_received, (void *)g_string_free(msg, FALSE), FALSE, NULL);
			XFlush(disp);
		}
	}
}

static void
hide_skype()
{
	return;
}

static void
exec_skype()
{
	return;
}