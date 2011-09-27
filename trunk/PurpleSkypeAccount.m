//
//  PurpleSkypeAccount.m
//  Adium
//
//  Created by Eion Robb on 2007-10-14.
//

#import "PurpleSkypeAccount.h"
#import <Adium/AIHTMLDecoder.h>
#import <Adium/AIStatusControllerProtocol.h>
#import <Adium/AIContentMessage.h>
#import <Adium/AIContentControllerProtocol.h>
#import <AdiumLibpurple/SLPurpleCocoaAdapter.h>
#import <Adium/AISharedAdium.h>

@implementation PurpleSkypeAccount

static SLPurpleCocoaAdapter *purpleThread = nil;

- (SLPurpleCocoaAdapter *)purpleThread
{
	//initialise libpurple in a safe way
	if (!purpleThread) {
		purpleThread = [[SLPurpleCocoaAdapter sharedInstance] retain];	
	}	
	return purpleThread;
}

- (const char*)protocolPlugin
{
#ifdef SKYPENET
	return "prpl-bigbrownchunx-skypenet";
#else
    return "prpl-bigbrownchunx-skype";
#endif
}

- (BOOL)disconnectOnFastUserSwitch
{
	return YES;
}

- (NSAttributedString *)statusMessageForPurpleBuddy:(PurpleBuddy *)buddy
{
	char *msg = (char *)skype_status_text(buddy);
	if (msg != NULL)
	{
		return [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:msg]];
	}
	else
		return nil;
}

- (NSString *)statusNameForPurpleBuddy:(PurpleBuddy *)buddy
{
	//printf("statusNameForPurpleBuddy cb\n");
	if (!buddy)
		return nil;
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	if (!presence)
		return nil;
	PurpleStatus *status = purple_presence_get_active_status(presence);
	if (!status)
		return nil;
		
	const gchar *status_id = purple_status_get_id(status);
	//printf("Buddy %s has status_id %s\n", buddy->name, status_id);
	if(g_str_equal(status_id, "ONLINE"))
		return STATUS_NAME_AVAILABLE;
	if(g_str_equal(status_id, "SKYPEME"))
		return STATUS_NAME_FREE_FOR_CHAT;
	if(g_str_equal(status_id, "AWAY"))
		return STATUS_NAME_AWAY;
	if(g_str_equal(status_id, "NA"))
		return STATUS_NAME_NOT_AVAILABLE;
	if(g_str_equal(status_id, "DND"))
		return STATUS_NAME_DND;
	if(g_str_equal(status_id, "INVISIBLE"))
		return STATUS_NAME_INVISIBLE;
	if(g_str_equal(status_id, "OFFLINE"))
		return STATUS_NAME_OFFLINE;
		
	BOOL show_skypeout_online = [[self preferenceForKey:KEY_SKYPE_SHOW_SKYPEOUT group:GROUP_ACCOUNT_STATUS] boolValue];
	//not sure what to do with SkypeOut people?
	if(g_str_equal(status_id, "SKYPEOUT"))
	{
		if (show_skypeout_online)
			return STATUS_NAME_AVAILABLE;
		else
			return STATUS_NAME_OFFLINE;
	}
	
	return STATUS_NAME_OFFLINE;
}

- (const char *)purpleStatusIDForStatus:(AIStatus *)statusState
							  arguments:(NSMutableDictionary *)arguments
{	
	if ([[statusState statusName] isEqualToString:STATUS_NAME_AVAILABLE])
		return "ONLINE";
	else if ([[statusState statusName] isEqualToString:STATUS_NAME_FREE_FOR_CHAT])
		return "SKYPEME";
	else if ([[statusState statusName] isEqualToString:STATUS_NAME_AWAY])
		return "AWAY";
	else if ([[statusState statusName] isEqualToString:STATUS_NAME_EXTENDED_AWAY])
		return "NA";
	else if ([[statusState statusName] isEqualToString:STATUS_NAME_DND])
		return "DND";
	else if ([[statusState statusName] isEqualToString:STATUS_NAME_INVISIBLE])
		return "INVISIBLE";
	else
		return "OFFLINE";
}

- (BOOL)connectivityBasedOnNetworkReachability
{
	return NO;
}

- (NSString *)connectionStringForStep:(NSInteger)step
{
	switch (step) {
		case 0:
			return @"Authorizing";
			break;
		case 1:
			return @"Initializing";
			break;
		case 2:
			return @"Silencing Skype";
			break;			
		case 3:
			return @"Connected";
			break;
	}

	return nil;
}
	
- (NSString *)encodedAttributedString:(NSAttributedString *)inAttributedString forListObject:(AIListObject *)inListObject
{
	/*if ([AIHTMLDecoder respondsToSelector:@selector(encodeHTML:headers:fontTags:includingColorTags:closeFontTags:styleTags:
													closeStyleTagsOnFontChange:encodeNonASCII:encodeSpaces:imagesPath:
													attachmentsAsText:onlyIncludeOutgoingImages:simpleTagsOnly:bodyBackground:
													allowJavascriptURLs:)])
	{*/
		NSString *temp = [AIHTMLDecoder encodeHTML:inAttributedString
							 headers:YES
							fontTags:NO
				  includingColorTags:NO
					   closeFontTags:NO
						   styleTags:NO
		  closeStyleTagsOnFontChange:NO
					  encodeNonASCII:NO
						encodeSpaces:NO
						  imagesPath:nil
				   attachmentsAsText:YES
		   onlyIncludeOutgoingImages:NO
					  simpleTagsOnly:YES
					  bodyBackground:NO
				 allowJavascriptURLs:NO];
		//printf("First encoder: %s\n", [temp cString]);
		return temp;
	/*} else {
		NSString *temp = [AIHTMLDecoder encodeHTML:inAttributedString
							 headers:YES
							fontTags:NO
				  includingColorTags:NO
					   closeFontTags:NO
						   styleTags:NO
		  closeStyleTagsOnFontChange:NO
					  encodeNonASCII:NO
						encodeSpaces:NO
						  imagesPath:nil
				   attachmentsAsText:YES
		   onlyIncludeOutgoingImages:NO
					  simpleTagsOnly:YES
					  bodyBackground:NO];
		printf("Second encoder: %s\n", [temp cString]);
		return temp;
	}*/
	/*return [AIHTMLDecoder encodeHTML:inAttributedString
					encodeFullString:YES];*/
}

/*
- (AIReconnectDelayType)shouldAttemptReconnectAfterDisconnectionError:(NSString **)disconnectionError
{
	//AIReconnectDelayType shouldAttemptReconnect = [super shouldAttemptReconnectAfterDisconnectionError:disconnectionError];

	if (disconnectionError && *disconnectionError) {
		if ([*disconnectionError rangeOfString:@"The password provided is incorrect"].location != NSNotFound) {
			[self setLastDisconnectionError:AILocalizedString(@"Incorrect username or password","Error message displayed when the server reports username or password as being incorrect.")];
			[self serverReportedInvalidPassword];
			shouldAttemptReconnect = AIReconnectImmediately;
		}
	}
	return AIReconnectNever;
	//return shouldAttemptReconnect;
}*/

- (void)autoReconnectAfterDelay:(NSTimeInterval)delay
{
	[super autoReconnectAfterDelay:5];
}

- (void)receivedIMChatMessage:(NSDictionary *)messageDict inChat:(AIChat *)chat
{
	PurpleMessageFlags flags = [[messageDict objectForKey:@"PurpleMessageFlags"] intValue];	
	//if its not a _SEND message let parent method deal with it
	if ((flags & PURPLE_MESSAGE_SEND) == 0) {
		return [super receivedIMChatMessage:messageDict
									 inChat:chat];
	}
	
	//otherwise, add this message from ourselves to the right IM window
	NSAttributedString *attributedMessage = [[adium contentController] decodedIncomingMessage:[messageDict objectForKey:@"Message"]
																				  fromContact:nil
																					onAccount:self];
	AIContentMessage *messageObject = [AIContentMessage messageInChat:chat
														   withSource:self
														  destination:self
																 date:[messageDict objectForKey:@"Date"]
															  message:attributedMessage
															autoreply:(flags & PURPLE_MESSAGE_AUTO_RESP) != 0];
	
	[[adium contentController] displayContentObject:messageObject
														 usingContentFilters:YES
																 immediately:NO];
}


- (BOOL)canSendOfflineMessageToContact:(AIListContact *)inContact
{
	//shortcut parent method
	return YES;
}

#ifndef SKYPENET
- (const char *)purpleAccountName
{
	//override parent method to grab the actual account UID from skype
	const char *uid = (const char *)skype_get_account_username(NULL);
	if (!uid || !strlen(uid))
	{
		return "Skype";
	}
	return uid;
}

- (NSString *)formattedUID
{
	const char *uid = (const char *)skype_get_account_username(NULL);
	if (!uid || !strlen(uid))
	{
		return @"Skype";
	}
	//override parent method to grab the actual account UID from skype
	return [[NSString alloc] initWithUTF8String:uid];
}
#endif

- (BOOL)shouldSetAliasesServerside
{
	return YES;
}

-(void)configurePurpleAccount{
	[super configurePurpleAccount];
	
	if (!account)
		return;
	
	//purple_account_set_bool(account, "skypeout_online", TRUE);
	//purple_account_set_bool(account, "skype_sync", TRUE);
	//purple_account_set_bool(account, "check_for_updates", FALSE);
	//purple_account_set_bool(account, "skype_autostart", TRUE);
	//return;
	
	//Use this section of code once the account options page is set up
	BOOL skypeout_online = [[self preferenceForKey:KEY_SKYPE_SHOW_SKYPEOUT group:GROUP_ACCOUNT_STATUS] boolValue];
	purple_account_set_bool(account, "skypeout_online", skypeout_online);
	
	BOOL skype_sync = [[self preferenceForKey:KEY_SKYPE_SYNC_OFFLINE group:GROUP_ACCOUNT_STATUS] boolValue];
	purple_account_set_bool(account, "skype_sync", skype_sync);
	
	BOOL check_for_updates = [[self preferenceForKey:KEY_SKYPE_CHECK_FOR_UPDATES group:GROUP_ACCOUNT_STATUS] boolValue];
	purple_account_set_bool(account, "check_for_updates", check_for_updates);
	
	BOOL skype_autostart = [[self preferenceForKey:KEY_SKYPE_AUTOSTART group:GROUP_ACCOUNT_STATUS] boolValue];
	purple_account_set_bool(account, "skype_autostart", skype_autostart);
}

-(NSDictionary *) extractChatCreationDictionaryFromConversation:(PurpleConversation *)conv
{
	
	NSMutableDictionary *dict = [NSMutableDictionary dictionary];
	[dict setObject:[NSString stringWithUTF8String:purple_conversation_get_name(conv)] forKey:@"chat_id"];
	const char *pass = purple_conversation_get_data(conv, "password");
	if (pass)
		[dict setObject: [NSString stringWithUTF8String:pass] forKey:@"password"];
	return dict;
}

- (BOOL)groupChatsSupportTopic
{
	return YES;
}

@end
