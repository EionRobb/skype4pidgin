//
//  PurpleSkypeAccountViewController.h
//  Adium
//
//  Created by Eion Robb on 14/10/07.
//

#import <Adium/AIAccountViewController.h>

@interface PurpleSkypeAccountViewController : AIAccountViewController {
    //IBOutlet	NSView			*view_options;              	//Account options (Host, port, mail, protocol, etc)
	
	IBOutlet		NSButton		*checkBox_skypeOutOnline;
	IBOutlet		NSButton		*checkBox_skypeSyncOffline;
	IBOutlet		NSButton		*checkBox_skypeCheckUpdates;
	IBOutlet		NSButton		*checkBox_autoStartSkype;
}

@end
