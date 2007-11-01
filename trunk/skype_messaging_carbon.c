#include <Carbon/Carbon.h>
#include <Skype/Skype.h>
#include <glib.h>
#include <CoreFoundation/CoreFoundation.h>

#define SENDSKYPERETURNS 1

static gboolean connected_to_skype = FALSE;

void SkypeNotificationReceived(CFStringRef input)
{
	printf("Message received\n");
	char *output = CFStringGetCStringPtr(input, kCFStringEncodingUTF8);
	if (!output)
	{
		output = NewPtr(CFStringGetLength(input)+1);
		CFStringGetCString(input, output, CFStringGetLength(input)+1, kCFStringEncodingUTF8);
	}
	
	g_thread_create((GThreadFunc)skype_message_received, (void *)output, FALSE, NULL);
}

void SkypeAttachResponse(unsigned int aAttachResponseCode)
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

void SkypeBecameAvailable(CFPropertyListRef aNotification)
{
	printf("Skype became available\n");
	connected_to_skype = TRUE;
}

void SkypeBecameUnavailable(CFPropertyListRef aNotification)
{
	printf("Skype became unavailable\n");
	connected_to_skype = FALSE;
}

static struct SkypeDelegate skypeDelegate = {
	CFSTR("Adium"), /* clientAppName */
	SkypeNotificationReceived,
	SkypeAttachResponse,
	SkypeBecameAvailable,
	SkypeBecameUnavailable
};

static gboolean skype_connect_thread(gpointer data)
{
	printf("Start inner event loop\n");
	RunApplicationEventLoop();
	printf("End of event loop\n");
	
	return FALSE;
}

static gboolean skype_connect()
{
	EventRef theEvent;
	EventTargetRef theTarget;
	EventRecord *eventRecord;
	gboolean is_skype_running = FALSE;
	
	is_skype_running = IsSkypeRunning();
	
	printf("Is Skype running? '%s'\n", is_skype_running?"Yes":"No");
	if (!is_skype_running)
		return FALSE;
		
	SetSkypeDelegate(&skypeDelegate);
	ConnectToSkype();
	
	g_thread_create((GThreadFunc)skype_connect_thread, NULL, FALSE, NULL);
	while(connected_to_skype == FALSE)
	{
		RunCurrentEventLoop(1);
	}
	printf("Connected to skype\n");
	return TRUE;
}

static void skype_disconnect()
{
	DisconnectFromSkype();
	RemoveSkypeDelegate();
	RunCurrentEventLoop(1);
}

static void send_message(char* message)
{
	printf("Skype send message\n");
#if SENDSKYPERETURNS
	CFStringRef returnString;
	returnString = SendSkypeCommand(CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8));
	if (returnString)
		SkypeNotificationReceived(returnString);
#else
	SendSkypeCommand(CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8));
#endif
}
