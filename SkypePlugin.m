//
//  SkypePlugin.m
//  SkypePlugin
//
//  Created by Eion Robb on 14/10/07.
//

#import "SkypePlugin.h"
#import "PurpleSkypeAccount.h"
#import "PurpleSkypeService.h"
#import <AdiumLibpurple/adiumPurpleFt.h>
#import <AdiumLibpurple/adiumPurpleEventloop.h>
#import <Adium/ESFileTransfer.h>
#import <libpurple/libpurple.h>
#include <dlfcn.h>

extern PurplePlugin *purple_plugin_new(gboolean native, const char *path) __attribute__((weak_import));
//extern void g_thread_init (GThreadFunctions *vtable) __attribute__((weak_import));

guint (*adium_timeout_add)(guint, GSourceFunc, gpointer);
gboolean purple_init_skype_plugin(void);

static void skypeAdiumPurpleAddXfer(PurpleXfer *xfer)
{
	if (!xfer || !xfer->account || strcmp(xfer->account->protocol_id, "prpl-bigbrownchunx-skype"))
		return;

	ESFileTransfer  *fileTransfer;
				
	//Ask the account for an ESFileTransfer* object
	fileTransfer = [accountLookup(xfer->account) newFileTransferObjectWith:[NSString stringWithUTF8String:(xfer->who)]
																	  size:purple_xfer_get_size(xfer)
															remoteFilename:[NSString stringWithUTF8String:(xfer->filename)]];
				
	//Configure the new object for the transfer
	[fileTransfer setAccountData:[NSValue valueWithPointer:xfer]];
				
	xfer->ui_data = [fileTransfer retain];
}

guint adium_threadsafe_timeout_add(guint interval, GSourceFunc function, gpointer data)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	guint return_val = adium_timeout_add(interval, function, data);
	[pool release];
	return return_val;
}

@implementation SkypePlugin

- (void)installPlugin
{
	printf("Installing SkypePlugin\n");
	
	//Check that we can thread stuff
	if (dlsym(RTLD_DEFAULT, "g_thread_init") == NULL)
	{
		printf("No g_thread_init();\n");
		//try opening thread libraries
		dlopen("~/Library/Application Support/Adium 2.0/PlugIns/SkypePlugin.AdiumLibpurplePlugin/Contents/Libraries/libgthread-2.0.0.dylib", RTLD_LAZY);
		if (dlsym(RTLD_DEFAULT, "g_thread_init") == NULL)
		{
			printf("Can't load GThread :(\n");
			return;
		}
	}

	printf("Calling purple_init_skype_plugin()\n");
	//Hack in our own transfer code and thread-safe timeout loop
	adium_purple_xfers_get_ui_ops()->add_xfer = skypeAdiumPurpleAddXfer;
    //adium_timeout_add = adium_purple_eventloop_get_ui_ops()->timeout_add;
	//adium_purple_eventloop_get_ui_ops()->timeout_add = adium_threadsafe_timeout_add;
#ifdef ENABLE_NLS
	//bindtextdomain("pidgin", [[[NSBundle bundleWithIdentifier:@"im.pidgin.libpurple"] resourcePath] fileSystemRepresentation]);
	//bind_textdomain_codeset("pidgin", "UTF-8");
	//textdomain("pidgin");
	
	bindtextdomain("skype4pidgin", [[[[NSBundle bundleWithIdentifier:@"org.bigbrownchunx.skypeplugin"] resourcePath] stringByAppendingPathComponent:@"/locale"] fileSystemRepresentation]);
	bind_textdomain_codeset("skype4pidgin", "UTF-8");
	textdomain("skype4pidgin");
#endif
	
	purple_init_skype_plugin();
	printf("Initialising PurpleSkypeService\n");
	SkypeService = [[PurpleSkypeService alloc] init];
}

- (void)uninstallPlugin
{
	[SkypeService release]; SkypeService = nil;
}

@end
