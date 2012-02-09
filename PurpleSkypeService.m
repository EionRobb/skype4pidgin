//
//  PurpleSkypeService.m
//  Adium
//
//  Created by Eion Robb on 14/10/07.
//

#import "PurpleSkypeService.h"
#import "PurpleSkypeAccount.h"
#import <AIUtilities/AIImageAdditions.h>
#import <Adium/DCJoinChatViewController.h>
#import <Adium/AIStatusControllerProtocol.h>
#import <Adium/AISharedAdium.h>

#import "SkypeJoinChatViewController.h"
#import "PurpleSkypeAccountViewController.h"

@implementation PurpleSkypeService

//Account Creation
- (Class)accountClass{
	return [PurpleSkypeAccount class];
}

- (AIAccountViewController *)accountViewController{
    return [PurpleSkypeAccountViewController accountViewController];
}

- (DCJoinChatViewController *)joinChatView{
	return [SkypeJoinChatViewController joinChatView];
}

#ifdef SKYPENET
- (BOOL)supportsProxySettings{
	return YES;
}

- (BOOL)supportsPassword
{
	return YES;
}

- (BOOL)requiresPassword
{
	return YES;
}
//Service Description
- (NSString *)serviceCodeUniqueID{
	return @"prpl-bigbrownchunx-skypenet";
}
- (NSString *)serviceID{
	return @"SkypeNet";
}
- (NSString *)serviceClass{
	return @"SkypeNet";
}
- (NSString *)shortDescription{
	return @"SkypeNet";
}
- (NSString *)longDescription{
	return @"SkypeNet";
}
#else
- (BOOL)supportsProxySettings{
	return NO;
}

- (BOOL)supportsPassword
{
	return NO;
}

- (BOOL)requiresPassword
{
	return NO;
}

- (NSString *)UIDPlaceholder
{
	return @"Skype";
}

//Service Description
- (NSString *)serviceCodeUniqueID{
	return @"prpl-bigbrownchunx-skype";
}
- (NSString *)serviceID{
	return @"Skype";
}
- (NSString *)serviceClass{
	return @"Skype";
}
- (NSString *)shortDescription{
	return @"Skype";
}
- (NSString *)longDescription{
	return @"Skype API";
}
#endif
- (NSCharacterSet *)allowedCharacters{
	return [[NSCharacterSet illegalCharacterSet] invertedSet];
}
- (NSCharacterSet *)ignoredCharacters{
	return [NSCharacterSet characterSetWithCharactersInString:@""];
}
- (NSUInteger)allowedLength{
	return 9999;
}
- (BOOL)caseSensitive{
	return NO;
}
- (AIServiceImportance)serviceImportance{
	return AIServicePrimary;
}
- (BOOL)canCreateGroupChats
{
	return YES;
}

/*- (void)registerStatuses
{
#define SKYPE_ADD_STATUS(status,statustype) [[adium statusController] registerStatus:status \
																	 withDescription:[[adium statusController] localizedDescriptionForCoreStatusName:status]\
																			  ofType:statustype \
																		  forService:self];
	SKYPE_ADD_STATUS(STATUS_NAME_AVAILABLE, AIAvailableStatusType);
	//SKYPE_ADD_STATUS(STATUS_NAME_FREE_FOR_CHAT, AIAvailableStatusType);
	[[adium statusController] registerStatus:STATUS_NAME_FREE_FOR_CHAT 
							 withDescription:@"SkypeMe"
									  ofType:AIAvailableStatusType
								  forService:self];
	SKYPE_ADD_STATUS(STATUS_NAME_AWAY, AIAwayStatusType);
	SKYPE_ADD_STATUS(STATUS_NAME_EXTENDED_AWAY, AIAwayStatusType);
	SKYPE_ADD_STATUS(STATUS_NAME_DND, AIAwayStatusType);
	SKYPE_ADD_STATUS(STATUS_NAME_INVISIBLE, AIInvisibleStatusType);
	SKYPE_ADD_STATUS(STATUS_NAME_OFFLINE, AIOfflineStatusType);
}*/

/*!
 * @brief Default icon
 *
 * Service Icon packs should always include images for all the built-in Adium services.  This method allows external
 * service plugins to specify an image which will be used when the service icon pack does not specify one.  It will
 * also be useful if new services are added to Adium itself after a significant number of Service Icon packs exist
 * which do not yet have an image for this service.  If the active Service Icon pack provides an image for this service,
 * this method will not be called.
 *
 * The service should _not_ cache this icon internally; multiple calls should return unique NSImage objects.
 *
 * @param iconType The AIServiceIconType of the icon to return. This specifies the desired size of the icon.
 * @return NSImage to use for this service by default
 */
- (NSImage *)defaultServiceIconOfType:(AIServiceIconType)iconType
{
	NSImage *image;
	NSString *imagename;
	NSSize imagesize;
	
	if (iconType == AIServiceIconLarge)
	{
		imagename = @"skype";
		imagesize = NSMakeSize(48,48);
	} else {
		imagename = @"skype-small";
		imagesize = NSMakeSize(16,16);
	}
	
	image = [NSImage imageNamed:(imagename)
					   forClass:[self class] loadLazily:YES];
	[image setSize:imagesize];
	return image;
}

#ifndef SKYPENET
- (NSString *)defaultUserName {
	return @"Skype"; 
}
#endif

@end
