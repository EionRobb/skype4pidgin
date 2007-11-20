

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>


typedef struct SkypeDelegate
{
	// Required member
	CFStringRef clientApplicationName;
	
	// Optional members, can be NULL
	void (*SkypeNotificationReceived)(CFStringRef aNotificationString);
	void (*SkypeAttachResponse)(unsigned int aAttachResponseCode);			// 0 - failed, 1 - success
	void (*SkypeBecameAvailable)(CFPropertyListRef aNotification);
	void (*SkypeBecameUnavailable)(CFPropertyListRef aNotification);
} SkypeDelegate;

static SkypeDelegate *delegate = NULL;
static int isavailable = 0;
static int client_id = 0;

char *
CFStringToCString(CFStringRef input)
{
	if (input == NULL)
		return NULL;
	int strlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(input), kCFStringEncodingUTF8);
	char *output = NewPtr(strlen+1);
	CFStringGetCString(input, output, strlen+1, kCFStringEncodingUTF8);
	return output;
}

int
CFNumberToCInt(CFNumberRef input)
{
	if (input == NULL)
		return NULL;
	int output;
	CFNumberGetValue(input, kCFNumberIntType, &output);
	return output;
}

void
availabilityUpdateCallback(
	CFNotificationCenterRef center,
	void *observer,
	CFStringRef name,
	const void *object,
	CFDictionaryRef userInfo)
{
	isavailable = CFNumberToCInt((CFNumberRef)CFDictionaryGetValue(userInfo, CFSTR("SKYPE_API_AVAILABILITY")));
}

void
debugCallback(
	CFNotificationCenterRef center,
	void *observer,
	CFStringRef name,
	const void *object,
	CFDictionaryRef userInfo)
{
	printf("Debug callback: %s\n", CFStringToCString(name));
	if (!userInfo)
		return;
	
	CFIndex count = CFDictionaryGetCount(userInfo);
	const void *keys[count];
	const void *values[count];
	CFDictionaryGetKeysAndValues(userInfo, keys, values);
	for(int i=0; i<count; i++)
	{
		printf("For i=%d, key: %s\n", i,
					CFStringToCString((CFStringRef)keys[i]));
	}
}


void
apiNotificationCallback(
	CFNotificationCenterRef center,
	void *observer,
	CFStringRef name,
	const void *object,
	CFDictionaryRef userInfo)
{
	int client_number = CFNumberToCInt((CFNumberRef)CFDictionaryGetValue(userInfo, CFSTR("SKYPE_API_CLIENT_ID")));
	if (client_number != 999 && (!client_id || client_id != client_number))
	{
		return;
	}
	CFStringRef string = (CFStringRef) CFDictionaryGetValue(userInfo, CFSTR("SKYPE_API_NOTIFICATION_STRING"));
	if (string && delegate && delegate->SkypeNotificationReceived)
	{
		delegate->SkypeNotificationReceived(string);
	}
}


void
attachResponseCallback(
	CFNotificationCenterRef center,
	void *observer,
	CFStringRef name,
	const void *object,
	CFDictionaryRef userInfo)
{
	int response = CFNumberToCInt((CFNumberRef)CFDictionaryGetValue(userInfo, CFSTR("SKYPE_API_ATTACH_RESPONSE")));
	if (response)
	{
		client_id = response;
		if (delegate && delegate->SkypeAttachResponse)
		{
			delegate->SkypeAttachResponse(response);
		}
	}
}


void
skypeQuitCallback(
	CFNotificationCenterRef center,
	void *observer,
	CFStringRef name,
	const void *object,
	CFDictionaryRef userInfo)
{
	if (delegate && delegate->SkypeBecameAvailable)
		delegate->SkypeBecameUnavailable(NULL);
}


void
skypeAvailableCallback(
	CFNotificationCenterRef center,
	void *observer,
	CFStringRef name,
	const void *object,
	CFDictionaryRef userInfo)
{
	if (delegate && delegate->SkypeBecameAvailable)
		delegate->SkypeBecameAvailable(NULL);
}


// STANDARD SKYPE.H BITS:


void
SetSkypeDelegate(SkypeDelegate *aDelegate)
{
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
	
	if (delegate == NULL)
	{
		delegate = aDelegate;

		CFNotificationCenterAddObserver(
			center,
			delegate->clientApplicationName,
			apiNotificationCallback,
			CFSTR("SKSkypeAPINotification"),
			NULL,
			CFNotificationSuspensionBehaviorDeliverImmediately);

		CFNotificationCenterAddObserver(
			center,
			delegate->clientApplicationName,
			skypeQuitCallback,
			CFSTR("SKSkypeWillQuit"),
			NULL,
			CFNotificationSuspensionBehaviorDeliverImmediately);

		CFNotificationCenterAddObserver(
			center,
			delegate->clientApplicationName,
			skypeAvailableCallback,
			CFSTR("SKSkypeBecameAvailable"),
			NULL,
			CFNotificationSuspensionBehaviorDeliverImmediately);
	}
}

SkypeDelegate *
GetSkypeDelegate(void)
{
	return delegate;
}


void
RemoveSkypeDelegate(void)
{
	delegate = NULL;
}

int IsSkypeAvailable(void)
{
	//is skype available?
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();

	CFNotificationCenterAddObserver(
		center,
		delegate->clientApplicationName,
		availabilityUpdateCallback,
		CFSTR("SKAvailabilityUpdate"),
		NULL,
		CFNotificationSuspensionBehaviorDeliverImmediately);

	CFNotificationCenterPostNotification(
		center,
		CFSTR("SKSkypeAPIAvailabilityRequest"),
		NULL,
		NULL,
		TRUE);
	
	RunCurrentEventLoop(1);
	int avail = isavailable;
	isavailable = 0;
	return avail;
}

int IsSkypeRunning(void)
{
	return 1;
}

void
ConnectToSkype(void)
{
	if (!delegate || !delegate->clientApplicationName)
	{
		printf("Error: Delegate not set\n");
		return;
	}
	
	if (!IsSkypeAvailable())
	{
		printf("Error: Skype not available\n");
	}

	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();

	CFNotificationCenterAddObserver(
		center,
		delegate->clientApplicationName,
		attachResponseCallback,
		CFSTR("SKSkypeAttachResponse"),
		NULL,
		CFNotificationSuspensionBehaviorDeliverImmediately);
			
	//do the connect
	CFNotificationCenterPostNotification(
		center,
		CFSTR("SKSkypeAPIAttachRequest"),
		delegate->clientApplicationName,
		NULL,
		TRUE);
}

void SendSkypeCommand(CFStringRef command)
{
	CFNumberRef id_number = CFNumberCreate(NULL, kCFNumberIntType, &client_id);
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
	const void **keys = {CFSTR("SKYPE_API_NOTIFICATION_STRING"), CFSTR("SKYPE_API_CLIENT_ID")};
	const void **values = {command, id_number};
	CFDictionaryRef userInfo = CFDictionaryCreate(NULL, keys, values, 2, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);	
	
	//send message
	CFNotificationCenterPostNotification(
		center,
		CFSTR("SKSkypeAPICommand"),
		NULL,
		userInfo,
		TRUE);
}

void DisconnectFromSkype(void)
{
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
	
	//disconnect
	CFNotificationCenterPostNotification(
		center,
		CFSTR("SKSkypeAPIDetachRequest"),
		NULL,
		NULL,
		TRUE);
		
	client_id = 0;
}
