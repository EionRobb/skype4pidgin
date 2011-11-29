/*
 * Skype plugin for libpurple/Pidgin/Adium
 * Written by: Eion Robb <eionrobb@gmail.com>
 *
 * This plugin uses the Skype API to show your contacts in libpurple, and send/receive
 * chat messages.
 * It requires the Skype program to be running.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCLUDED_LIBSKYPE_C 
#	error	Don't compile this file directly.  Just compile libskype.c instead.
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Display *disp = NULL;
static Window win = (Window)None;
Window skype_win = (Window)None;
static GThread *receiving_thread = NULL;
Atom message_start, message_continue;
static gboolean run_loop = FALSE;
static unsigned char x11_error_code = 0;
static GStaticMutex x11_mutex = G_STATIC_MUTEX_INIT;

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
	
	x11_error_code = 0;
	XSetErrorHandler(x11_error_handler);
	skype_debug_info("skype_x11", "Set the XErrorHandler\n");
#ifdef USE_XVFB_SERVER	
	if (!getenv("SKYPEDISPLAY"))
		setenv("SKYPEDISPLAY", ":25", 0);
#endif
	if (getenv("SKYPEDISPLAY"))
		disp = XOpenDisplay(getenv("SKYPEDISPLAY"));
	else
		disp = XOpenDisplay(getenv("DISPLAY"));
	if (disp == NULL)
	{
		skype_debug_info("skype_x11", "Couldn't open display\n");
		return FALSE;
	}
	skype_debug_info("skype_x11", "Opened display\n");
	message_start = XInternAtom( disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False );
	message_continue = XInternAtom( disp, "SKYPECONTROLAPI_MESSAGE", False );
	root = DefaultRootWindow( disp );
	win = XCreateSimpleWindow( disp, root, 0, 0, 1, 1,
		0, BlackPixel( disp, DefaultScreen( disp ) ),
		BlackPixel( disp, DefaultScreen( disp ) ));
	XFlush(disp);
	if (win == None)
	{
		XCloseDisplay(disp);
		skype_debug_info("skype_x11", "Could not create X11 messaging window\n");
		return FALSE;
	}
	skype_debug_info("skype_x11", "Created X11 messaging window\n");
	skype_inst = XInternAtom(disp, "_SKYPE_INSTANCE", True);
	if (skype_inst == None)
	{
		XDestroyWindow(disp, win);
		XCloseDisplay(disp);
		win = (Window)None;
		skype_win = (Window)None;
		skype_debug_info("skype_x11", "Could not create skype Atom\n");
		return FALSE;
	}
	skype_debug_info("skype_x11", "Created skype Atom\n");
	status = XGetWindowProperty(disp, root, skype_inst, 0, 1, False, XA_WINDOW, &type_ret, &format_ret, &nitems_ret, &bytes_after_ret, &prop);
	if(status != Success || format_ret != 32 || nitems_ret < 1)
	{
		XDestroyWindow(disp, win);
		XCloseDisplay(disp);
		win = (Window)None;
		XFree(prop);
		skype_win = (Window)None;
		skype_debug_info("skype", "Skype instance not found\n");
		return FALSE;
	}
	skype_debug_info("skype_x11", "Skype instance found\n");
	skype_win = * (const unsigned long *) prop & 0xffffffff;
	XFree(prop);
	run_loop = TRUE;
	
	skype_debug_info("skype_x11", "Charging lasers...\n");
	receiving_thread = g_thread_create((GThreadFunc)receive_message_loop, NULL, FALSE, NULL);
	
	return TRUE;
}


static void
skype_disconnect()
{
	run_loop = FALSE;
	skype_win = (Window)None;
	
	if (disp != NULL)
	{
		if (win != None)
		{			
			//wait here for the event to be handled
			XDestroyWindow(disp, win);
		}
		XCloseDisplay(disp);
	}
		
	win = (Window)None;
	disp = NULL;
}

int
x11_error_handler(Display *disp, XErrorEvent *error)
{
	x11_error_code = error->error_code;
	return FALSE;
}


static void
send_message(const char* message)
{
	unsigned int pos = 0;
	unsigned int len = strlen( message );
	XEvent e;
	int message_num;
	char *error_return;
	unsigned int i;
	
	if (skype_win == None || win == None || disp == NULL)
	{
		//There was an error
		if (message[0] == '#')
		{
			//And we're expecting a response
			sscanf(message, "#%d ", &message_num);
			error_return = g_strdup_printf("#%d ERROR X11", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)error_return, FALSE, NULL);
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
		g_static_mutex_lock(&x11_mutex);
		XSendEvent( disp, skype_win, False, 0, &e );
		g_static_mutex_unlock(&x11_mutex);

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
			error_return = g_strdup_printf("#%d ERROR X11_2", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)error_return, FALSE, NULL);
		}
		g_thread_create((GThreadFunc)skype_message_received, g_strdup("CONNSTATUS LOGGEDOUT"), FALSE, NULL); 
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

	skype_debug_info("skype_x11", "receive_message_loop started\n");
	
	msg_temp[20] = '\0';
	XSetErrorHandler(x11_error_handler);
	while(run_loop)
	{
		if (!disp)
		{
			skype_debug_error("skype_x11", "display has disappeared\n");
			g_thread_create((GThreadFunc)skype_message_received, g_strdup("CONNSTATUS LOGGEDOUT"), FALSE, NULL);
			break;
		}
		
		Bool event_bool;
		g_static_mutex_lock(&x11_mutex);
		event_bool = XCheckTypedEvent(disp, ClientMessage, &e);
		g_static_mutex_unlock(&x11_mutex);
		if (!event_bool)
		{
			g_thread_yield();
			usleep(1000);
			//XPeekEvent(disp, &e);
			//sleep(1);
			continue;
		}/*
		XNextEvent(disp, &e);
		printf("skype event: %d  (clientmessage: %d)\n", e.type, ClientMessage);
		if (e.type != ClientMessage)
			continue;*/
		
		strncpy(msg_temp, e.xclient.data.b, 20);
		len = strlen(msg_temp);
		if (e.xclient.message_type == message_start)
			msg = g_string_new_len(msg_temp, len);
		else if (e.xclient.message_type == message_continue)
			msg = g_string_append_len(msg, msg_temp, len);
		else
		{
			skype_debug_info("skype_x11", "unknown message type: %d\n", e.xclient.message_type);
			if (disp)
			{
				g_static_mutex_lock(&x11_mutex);
				XFlush(disp);
				g_static_mutex_unlock(&x11_mutex);
			}
			continue;
		}
		if (len < 20)
		{
			g_thread_create((GThreadFunc)skype_message_received, (void *)g_string_free(msg, FALSE), FALSE, NULL);
			if (disp)
			{
				g_static_mutex_lock(&x11_mutex);
				XFlush(disp);
				g_static_mutex_unlock(&x11_mutex);
			}
		}
	}
}

static void
hide_skype()
{
	
}

static gboolean
exec_skype()
{
#ifdef INSTANTBIRD
	return FALSE;
#else
	GError *error;
	
#ifdef USE_XVFB_SERVER	
	PurpleAccount *acct = NULL;
	int skype_stdin;
	gchar **skype_list;
	gchar *command;
	
	if (!getenv("SKYPEDISPLAY"))
		setenv("SKYPEDISPLAY", ":25", 0);
	unsetenv("DBUS_SESSION_BUS_ADDRESS");
	command = g_strconcat("Xvfb ",
				//"Xnest ", //Uncomment if using Xnest
				getenv("SKYPEDISPLAY"),
				" -ac -terminate -tst -xinerama",
				" -render -shmem -screen 0 320x240x16", //Dont use me if using Xnest
				NULL);
	if (g_spawn_command_line_async(command, NULL))
	{
		acct = skype_get_account(NULL);
		skype_debug_info("skype_x11", "acct: %d\n", acct);
		if (acct && acct->username && acct->username[0] != '\0' &&
			acct->password && acct->password[0] != '\0')
		{
			g_free(command);
			command = g_strconcat("skype --pipelogin -display ",
						getenv("SKYPEDISPLAY"),
						NULL);
			g_shell_parse_argv(command, NULL, &skype_list, NULL);
			if (g_spawn_async_with_pipes(NULL, skype_list, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &skype_stdin, NULL, NULL, NULL))
			{
				g_strfreev(skype_list);
				write(skype_stdin, acct->username, strlen(acct->username));
				write(skype_stdin, " ", 1);
				write(skype_stdin, acct->password, strlen(acct->password));
				write(skype_stdin, "\n", 1);
				fsync(skype_stdin);
				skype_debug_info("skype_x11", "pipelogin worked\n");
				g_free(command);
				return TRUE;
			}
			g_strfreev(skype_list);
		}
	}
	g_free(command);
#endif
	
	if (g_spawn_command_line_async("skype --disable-cleanlooks", &error))
	{
		return TRUE;
	} else {
		skype_debug_error("skype", "Could not start skype: %s\n", error->message);
		return FALSE;
	}
#endif
}

static gboolean
is_skype_running()
{
	const gchar *temp;
	int pid;
	gchar* stat_path;
	FILE *fh;
	gchar exec_name[16];
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
		if (!fh)
		{
			g_free(stat_path);
			continue;
		}
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

