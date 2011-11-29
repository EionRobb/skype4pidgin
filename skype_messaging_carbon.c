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

#include <Carbon/Carbon.h>
//#include <Skype/Skype.h>
#include <glib.h>
#include <CoreFoundation/CoreFoundation.h>

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
	int strlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(input), kCFStringEncodingUTF8);
	
	output = (char *)CFStringGetCStringPtr(input, kCFStringEncodingUTF8);
	if (!output)
	{
		output = NewPtr(strlen+1);
		CFStringGetCString(input, output, strlen+1, kCFStringEncodingUTF8);
	}
	//printf("Message received  %s\n", output);
	skype_message_received(output);
	if (error)
	{
		skype_debug_error("skype_osx", "Could not create new thread!!! %s\n", error->message);
		g_error_free(error);
	}
}

void
SkypeAttachResponse(unsigned int aAttachResponseCode)
{
	if (aAttachResponseCode)
	{
		skype_debug_info("skype_osx", "Skype attached successfully :)\n");
		connected_to_skype = TRUE;
	}
	else
	{
		skype_debug_info("skype_osx", "Skype couldn't connect :(\n");
		connected_to_skype = FALSE;
	}
}

void
SkypeBecameAvailable(CFPropertyListRef aNotification)
{
	skype_debug_info("skype_osx", "Skype became available\n");
	//connected_to_skype = TRUE;
	allow_app_in_skype_api();
}

void
SkypeBecameUnavailable(CFPropertyListRef aNotification)
{
	skype_debug_info("skype_osx", "Skype became unavailable\n");
	connected_to_skype = FALSE;
	g_thread_create((GThreadFunc)skype_message_received, g_strdup("CONNSTATUS LOGGEDOUT"), FALSE, NULL);
}

static struct SkypeDelegate skypeDelegate = {
	CFSTR("Adium"), /* clientAppName */
	SkypeNotificationReceived,
	SkypeAttachResponse,
	SkypeBecameAvailable,
	SkypeBecameUnavailable
};

static gboolean
skype_connect()
{
	gboolean is_skype_running = FALSE;
	int timeout_count = 0;
	
	is_skype_running = IsSkypeRunning();
	
	skype_debug_info("skype_osx", "Is Skype running? '%s'\n", (is_skype_running?"Yes":"No"));
	if (!is_skype_running)
		return FALSE;
	
	if (connected_to_skype)
		skype_disconnect();
	
	SetSkypeDelegate(&skypeDelegate);
	RunCurrentEventLoop(1);
	ConnectToSkype();
	
	while(connected_to_skype == FALSE)
	{
		RunCurrentEventLoop(1);
		allow_app_in_skype_api();
		if (timeout_count++ == 8)
			return FALSE;
	}
	skype_debug_info("skype_osx", "Connected to skype\n");
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
send_message(const char* message)
{
	if (!connected_to_skype)
	{
		if (message[0] == '#')
		{
			int message_num;
			//And we're expecting a response
			sscanf(message, "#%d ", &message_num);
			char *error_return = g_strdup_printf("#%d ERROR Carbon", message_num);
			g_thread_create((GThreadFunc)skype_message_received, (void *)error_return, FALSE, NULL);
		}
		return;
	}

	CFStringRef messageString = CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8);
#if SENDSKYPERETURNS
	CFStringRef returnString = NULL;
	returnString = SendSkypeCommand(messageString);
	if (returnString)
		SkypeNotificationReceived(returnString);
#else
	SendSkypeCommand(messageString);
#endif
	//printf("Skype send message %s\n", message);
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
#if __LP64__
	info.processAppRef = NULL;
#else
	info.processAppSpec = NULL;
#endif
	
	while(status == noErr)
	{
		for(i = 0; i < procNameLength; i++)
			procName[i] = '\0';
		
		status = GetNextProcess(&psn);
		if (status == noErr)
			if (GetProcessInformation(&psn, &info) == noErr)
				//for some reason first character is poisioned
				if (g_str_equal((char *)&procName[1], "Skype"))
				{
					ShowHideProcess(&psn, FALSE);
					return;
				}
	}
}

static gboolean
exec_skype()
{
	gboolean success = FALSE;
#ifndef INSTANTBIRD	
	success = g_spawn_command_line_async("/Applications/Skype.app/Contents/MacOS/Skype", NULL);
#endif	
	return success;
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
														"click button 4\n"
													"end tell\n"
													"exit repeat\n"
												"end if\n"
											"end repeat\n"
										"end tell";
	AEDesc script_data;
	OSAID script_id = kOSANullScript;
	OSAError err;
	FILE *access_file = NULL;
	
	skype_debug_info("skype_osx", "Enabling universal access\n");
	access_file = fopen("/private/var/db/.AccessibilityAPIEnabled", "w");
	if (access_file != NULL)
	{
		fwrite("a\n", 1, 2, access_file);
		fclose(access_file);
	}
	
	ComponentInstance script = OpenDefaultComponent(kOSAComponentType, typeAppleScript);
	AECreateDesc(typeChar, script_string, strlen(script_string), &script_data);
	OSACompile(script, &script_data, kOSAModeNull, &script_id);
	skype_debug_info("skype_osx", "Trying to run AppleScript code\n");
	err = OSAExecute(script, script_id, kOSANullScript, kOSAModeNull, &script_id);
	if (err == -1753)
	{
		skype_debug_error("skype_osx", "Error: 'Access assistive devices' isn't enabled\n"
				"see http://images.apple.com/applescript/uiscripting/gfx/gui.03.jpg for details.\n");
	} else {
		skype_debug_info("skype_osx", "Error number %d\n", err);
	}
}	

