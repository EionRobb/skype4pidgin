

#import <Adium/DCJoinChatViewController.h>

@class AIAccount, AICompletingTextField;

@interface SkypeJoinChatViewController  : DCJoinChatViewController {
	//IBOutlet		NSView			*view;			// Custom view
	
	IBOutlet		NSTextField		*textField_roomName;
	IBOutlet		NSTextField		*textField_password;
	
	IBOutlet		AICompletingTextField		*textField_inviteUsers;
}

- (void)setRoomName:(NSString*)roomName;

@end
