#include <Carbon/Carbon.h>
//#include <Skype/Skype.h>
#include <glib.h>
#include <CoreFoundation/CoreFoundation.h>

#include "AutoreleasePoolInit.h"

//change this to 0 if using an old version of the Skype.framework
#define SENDSKYPERETURNS 0

#include "skype_messaging_carbon2.c"

static gboolean connected_to_skype = FALSE;


static void allow_app_in_skype_api(void);

void
SkypeNotificationReceived(CFStringRef input)
{
	char *output = NULL;
	GError *error = NULL;
	void *pool = initAutoreleasePool();
	int strlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(input), kCFStringEncodingUTF8);
	
	printf("Message received");
	output = (char *)CFStringGetCStringPtr(input, kCFStringEncodingUTF8);
	if (!output)
	{
		output = NewPtr(strlen+1);
		CFStringGetCString(input, output, strlen+1, kCFStringEncodingUTF8);
	}
	printf(" %s\n", output);
	//g_thread_create((GThreadFunc)skype_message_received, (void *)output, FALSE, &error);
	skype_message_received(output);
	if (error)
	{
		printf("Could not create new thread!!! %s\n", error->message);
		g_error_free(error);
	}
	
	destroyAutoreleasePool(pool);
}

void
SkypeAttachResponse(unsigned int aAttachResponseCode)
{
	if (aAttachResponseCode)
	{
		printf("Skype attached successfully :)\n");
		connected_to_skype = TRUE;
	}
	else
	{
		printf("Skype couldn't connect :(\n");
		connected_to_skype = FALSE;
	}
}

void
SkypeBecameAvailable(CFPropertyListRef aNotification)
{
	printf("Skype became available\n");
	//connected_to_skype = TRUE;
	allow_app_in_skype_api();
}

void
SkypeBecameUnavailable(CFPropertyListRef aNotification)
{
	printf("Skype became unavailable\n");
	connected_to_skype = FALSE;
	g_thread_create((GThreadFunc)skype_message_received, "CONNSTATUS LOGGEDOUT", FALSE, NULL);
}

static struct SkypeDelegate skypeDelegate = {
	CFSTR("Adium"), /* clientAppName */
	SkypeNotificationReceived,
	SkypeAttachResponse,
	SkypeBecameAvailable,
	SkypeBecameUnavailable
};

/*static gboolean
skype_connect_thread(gpointer data)
{
	static gboolean started = FALSE;
	if (started)
		return FALSE;
	started = TRUE;
	
	void *pool = initAutoreleasePool();

	printf("Start inner event loop\n");
	while(true)
	{
		//RunApplicationEventLoop();
		RunCurrentEventLoop(1);
	}
	printf("End of event loop\n");
	started = FALSE;
	
	destroyAutoreleasePool(pool);
	
	//don't loop this thread
	return FALSE;
}*/

static gpointer static_pool;

static gboolean
skype_connect()
{
	gboolean is_skype_running = FALSE;
	
	if (!static_pool)
		static_pool = initAutoreleasePool();
	
	is_skype_running = IsSkypeRunning();
	
	printf("Is Skype running? '%s'\n", (is_skype_running?"Yes":"No"));
	if (!is_skype_running)
		return FALSE;
	
	if (connected_to_skype)
		skype_disconnect();
	
	SetSkypeDelegate(&skypeDelegate);
	
	//g_thread_create((GThreadFunc)skype_connect_thread, NULL, FALSE, NULL);
	while(connected_to_skype == FALSE)
	{
		ConnectToSkype();
		RunCurrentEventLoop(1);
	}
	printf("Connected to skype\n");
	return TRUE;
}

static void
skype_disconnect()
{	
	connected_to_skype = FALSE;
	DisconnectFromSkype();
	RemoveSkypeDelegate();
	RunCurrentEventLoop(1);
}

static void
send_message(char* message)
{
	CFStringRef messageString = CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8);
	if (!connected_to_skype)
	{
		if (message[0] == '#')
		{
			int message_num;
			char error_return[40];
			//And we're expecting a response
			sscanf(message, "#%d ", &message_num);
			sprintf(error_return, "#%d ERROR", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)g_strdup(error_return), FALSE, NULL);
		}
		CFRelease(messageString);
		return;
	}

	gpointer pool = initAutoreleasePool();
	printf("Skype send message ");
#if SENDSKYPERETURNS
	CFStringRef returnString = NULL;
	returnString = SendSkypeCommand(messageString);
	if (returnString)
		SkypeNotificationReceived(returnString);
#else
	SendSkypeCommand(messageString);
#endif
	destroyAutoreleasePool(pool);
	printf("%s\n", message);
	CFRelease(messageString);
}

static void
hide_skype()
{
	OSStatus status = noErr;
	ProcessSerialNumber psn = {kNoProcess, kNoProcess};
	unsigned int procNameLength = 32;
	unsigned char procName[procNameLength];
	unsigned int i = 0;
	ProcessInfoRec info;
	info.processInfoLength = sizeof(ProcessInfoRec);
	info.processName = procName;
	info.processAppSpec = NULL;
	
	while(status == noErr)
	{
		for(i = 0; i < procNameLength; i++)
			procName[i] = '\0';
		
		status = GetNextProcess(&psn);
		if (status == noErr)
			if (GetProcessInformation(&psn, &info) == noErr)
				//for some reason first character is poisioned
				if (strcmp((char *)&procName[1], "Skype") == 0)
				{
					ShowHideProcess(&psn, FALSE);
					return;
				}
	}
}

static gboolean
exec_skype()
{
	return g_spawn_command_line_async("/Applications/Skype.app/Contents/MacOS/Skype", NULL);
}

static gboolean
is_skype_running()
{
	return IsSkypeRunning();
}

static void
allow_app_in_skype_api()
{
	static const char *script_string = "tell application \"System Events\" to tell process \"Skype\"\n"
											"set numWindows to count of windows\n"
											"repeat with w from 1 to numWindows\n"
												"if (name of window w contains \"Skype\" and name of window w contains \"API\")\n"
													"tell window w\n"
														"click radio button 1 of radio group 1\n"
														"delay 0.1\n"
														"click button 1\n"
													"end tell\n"
													"exit repeat\n"
												"end if\n"
											"end repeat\n"
										"end tell"
	AEDesc script_data;
	OSAID script_id;
	OSAError err;
	
	printf("Enabling universal access\n");
	fclose(fopen("/private/var/db/.AccessibilityAPIEnabled", "w"));
	
	ComponentInstance script = OpenDefaultComponent(kOSAComponentType, typeAppleScript);
	AECreateDesc(typeChar, script_string, strlen(script_string), &script_data);
	OSACompile(script, &script_data, kOSAModeNull, &script_id);
	printf("Trying to run AppleScript code\n");
	err = OSAExecute(script, script_id, kOSANullScript, kOSAModeNull, &script_id);
	if (err == -1753)
	{
		printf("Error: 'Access assistive devices' isn't enabled\n"
				"see http://images.apple.com/applescript/uiscripting/gfx/gui.03.jpg for details.\n");
	}
}	

