

#import "SkypeJoinChatViewController.h"
#import <Adium/AIContactControllerProtocol.h>
#import <Adium/DCJoinChatWindowController.h>
#import <AIUtilities/AICompletingTextField.h>

/*@interface SkypeJoinChatViewController ()
- (void)_configureTextField;
@end*/

@implementation SkypeJoinChatViewController

- (id)init
{
	if ((self = [super init]))
	{
		[textField_inviteUsers setDragDelegate:self];
		[textField_inviteUsers registerForDraggedTypes:[NSArray arrayWithObjects:@"AIListObject", @"AIListObjectUniqueIDs", nil]];
	}	
	
	return self;
}

- (void)configureForAccount:(AIAccount *)inAccount
{	
	[super configureForAccount:inAccount];

	//[(DCJoinChatWindowController *)delegate setJoinChatEnabled:NO];
	[[view window] makeFirstResponder:textField_roomName];
	
	[textField_inviteUsers setMinStringLength:2];
	[textField_inviteUsers setCompletesOnlyAfterSeparator:YES];
	//[self _configureTextField];
}

- (void)joinChatWithAccount:(AIAccount *)inAccount
{	
	NSString		*room = [textField_roomName stringValue];
	NSString		*password = [textField_password stringValue];
	NSMutableDictionary	*chatCreationInfo;
	
	if (![room length]) return [super joinChatWithAccount:inAccount];

	if (![password length]) password = nil;
	
	chatCreationInfo = [NSMutableDictionary dictionaryWithObjectsAndKeys:
						room, @"chat_id",
						nil];

	if (password) {
		[chatCreationInfo setObject:password
							 forKey:@"password"];
	}

	[self doJoinChatWithName:room
				   onAccount:inAccount
			chatCreationInfo:chatCreationInfo
			invitingContacts:[self contactsFromNamesSeparatedByCommas:[textField_inviteUsers stringValue] onAccount:inAccount]
	   withInvitationMessage:nil];
}

- (NSString *)nibName
{
	return @"SkypeJoinChatView";
}

- (NSString *)impliedCompletion:(NSString *)aString
{
	return [textField_inviteUsers impliedStringValueForString:aString];
}
/*
- (void)_configureTextField
{
	NSEnumerator		*enumerator;
    AIListContact		*contact;
	
	//Clear the completing strings
	[textField_inviteUsers setCompletingStrings:nil];
	
	//Configure the auto-complete view to autocomplete for contacts matching the selected account's service
    enumerator = [[[[AIObject sharedAdiumInstance] contactController] allContacts] objectEnumerator];
    while ((contact = [enumerator nextObject])) {
		if (contact.service == account.service) {
			NSString *UID = contact.UID;
			[textField_inviteUsers addCompletionString:contact.formattedUID withImpliedCompletion:UID];
			[textField_inviteUsers addCompletionString:contact.displayName withImpliedCompletion:UID];
			[textField_inviteUsers addCompletionString:UID];
		}
    }
}*/

- (void)setRoomName:(NSString*)roomName {
	[textField_roomName setStringValue:roomName];
}

#pragma mark Dragging Delegate

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
	return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	return [super doPerformDragOperation:sender toField:textField_inviteUsers];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	return [super doDraggingEntered:sender];
}




@end
