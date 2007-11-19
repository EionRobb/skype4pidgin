

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

char *
CFStringToCString(CFStringRef input)
{
	int strlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(input), kCFStringEncodingUTF8);
	char *output = NewPtr(strlen+1);
	CFStringGetCString(input, output, strlen+1, kCFStringEncodingUTF8);
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
	CFNumberRef number = NULL;
	number = (CFNumberRef) CFDictionaryGetValue(userInfo, CFSTR("SKYPE_API_AVAILABILITY"));
	CFNumberGetValue(number, kCFNumberIntType, &isavailable);
	
	printf("Availability update: %d\n", isavailable);
}

void
debugCallback(
	CFNotificationCenterRef center,
	void *observer,
	CFStringRef name,
	const void *object,
	CFDictionaryRef userInfo)
{
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
	printf("Debug callback: %s\n", CFStringToCString(name));
}


void
apiNotificationCallback(
	CFNotificationCenterRef center,
	void *observer,
	CFStringRef name,
	const void *object,
	CFDictionaryRef userInfo)
{
	CFStringRef string = NULL;
	string = (CFStringRef) CFDictionaryGetValue(userInfo, CFSTR("SKYPE_API_NOTIFICATION_STRING"));
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
	const void *response = CFDictionaryGetValue(userInfo, CFSTR("SKYPE_API_ATTACH_RESPONSE"));
	if (response)
	{
		CFNumberRef number = (CFNumberRef) response;
		unsigned int response_num;
		
		CFNumberGetValue(number, kCFNumberIntType, &response_num);
		
		if (delegate && delegate->SkypeAttachResponse)
		{
			delegate->SkypeAttachResponse(response_num);
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
			attachResponseCallback,
			CFSTR("SKSkypeAttachResponse"),
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
	
	int timeout = 0;
	//wait for isavailable to not be -1;
	while(isavailable == 0 && timeout<2)
	{
		RunCurrentEventLoop(1);
		timeout++;
	}
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
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
	
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
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
	
	//send message
	CFNotificationCenterPostNotification(
		center,
		CFSTR("SKSkypeAPICommand"),
		NULL,
		NULL,
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
}