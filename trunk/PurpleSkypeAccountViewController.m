//
//  PurpleSkypeAccountViewController.m
//  Adium
//
//  Created by Eion Robb on 14/10/07.
//

#import "PurpleSkypeAccountViewController.h"
#import "PurpleSkypeAccount.h"

@implementation PurpleSkypeAccountViewController

- (NSView *)privacyView{
	return nil;
}

- (NSString *)nibName{
	return @"PurpleSkypeAccountView";
}

#ifndef SKYPENET
- (NSView *)setupView {
	return nil;
}
#endif

//Configure our controls
- (void)configureForAccount:(AIAccount *)inAccount
{
    [super configureForAccount:inAccount];
	
	[checkBox_skypeOutOnline setState:[[account preferenceForKey:KEY_SKYPE_SHOW_SKYPEOUT group:GROUP_ACCOUNT_STATUS] boolValue]];
	[checkBox_skypeSyncOffline setState:[[account preferenceForKey:KEY_SKYPE_SYNC_OFFLINE group:GROUP_ACCOUNT_STATUS] boolValue]];
	//[checkBox_skypeCheckUpdates setState:[[account preferenceForKey:KEY_SKYPE_CHECK_FOR_UPDATES group:GROUP_ACCOUNG_STATUS] boolValue]];
	[checkBox_autoStartSkype setState:[[account preferenceForKey:KEY_SKYPE_AUTOSTART group:GROUP_ACCOUNT_STATUS] boolValue]];
}

//Save controls
- (void)saveConfiguration
{
    [super saveConfiguration];
	
	[account setPreference:[NSNumber numberWithBool:[checkBox_skypeOutOnline state]]
					forKey:KEY_SKYPE_SHOW_SKYPEOUT group:GROUP_ACCOUNT_STATUS];
	[account setPreference:[NSNumber numberWithBool:[checkBox_skypeSyncOffline state]]
					forKey:KEY_SKYPE_SYNC_OFFLINE group:GROUP_ACCOUNT_STATUS];
	//[account setPreference:[NSNumber numberWithBool:[checkBox_skypeCheckUpdates state]]
	//				forKey:KEY_SKYPE_CHECK_FOR_UPDATES group:GROUP_ACCOUNT_STATUS];
	[account setPreference:[NSNumber numberWithBool:[checkBox_autoStartSkype state]]
					forKey:KEY_SKYPE_AUTOSTART group:GROUP_ACCOUNT_STATUS];
}

@end
