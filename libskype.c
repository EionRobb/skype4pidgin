/*
 * Skype plugin for libpurple/Pidgin/Adium
 * Written by: Eion Robb <bigbrownchunx@sf.net>
 *
 * This plugin uses the Skype API to show your contacts in libpurple, and send/receive
 * chat messages.
 * It requires the Skype program to be running. 
 *
 * Skype API Terms of Use:
 * The following statement must be displayed in the documentation of this appliction:
 * This plugin "uses Skype Software" to display contacts, and chat to Skype users from within Pidgin
 * "This product uses the Skype API but is not endorsed, certified or otherwise approved in any way by Skype"

 * The use of this plugin requries your acceptance of the Skype EULA (http://www.skype.com/intl/en/company/legal/eula/index.html)

 * Skype is the trademark of Skype Limited
 */

#define PURPLE_PLUGIN
#define DBUS_API_SUBJECT_TO_CHANGE

#include <glib.h>

#include <internal.h>

#include <notify.h>
#include <core.h>
#include <plugin.h>
#include <version.h>
#include <debug.h>      /* for purple_debug_info */
#include <accountopt.h>
#include <blist.h>
#include <request.h>

#include "skype_messaging.c"

static void plugin_init(PurplePlugin *plugin);
gboolean plugin_load(PurplePlugin *plugin);
gboolean plugin_unload(PurplePlugin *plugin);
GList *skype_status_types(PurpleAccount *acct);
void skype_login(PurpleAccount *acct);
void skype_close(PurpleConnection *gc);
int skype_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags);
static void skype_hide();
void skype_get_info(PurpleConnection *gc, const gchar *username);
gchar *skype_get_user_info(const gchar *username, const gchar *property);
void skype_set_status(PurpleAccount *account, PurpleStatus *status);
void skype_set_idle(PurpleConnection *gc, int time);
void skype_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void skype_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void skype_add_deny(PurpleConnection *gc, const char *who);
void skype_rem_deny(PurpleConnection *gc, const char *who);
void skype_add_permit(PurpleConnection *gc, const char *who);
void skype_rem_permit(PurpleConnection *gc, const char *who);
void skype_keepalive(PurpleConnection *gc);
gboolean skype_set_buddies(PurpleAccount *acct);
void skype_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img);
GList *skype_actions(PurplePlugin *plugin, gpointer context);
void skype_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *userinfo, gboolean full);
char *skype_status_text(PurpleBuddy *buddy);
const char *skype_list_icon(PurpleAccount *account, PurpleBuddy *buddy);
const char *skype_normalize(const PurpleAccount *acct, const char *who);
void skype_update_buddy_alias(PurpleBuddy *buddy);
gboolean skype_update_buddy_status(PurpleBuddy *buddy);
void skype_get_account_alias(PurpleAccount *acct);
char *skype_get_account_username(PurpleAccount *acct);
static PurpleAccount *skype_get_account(PurpleAccount *newaccount);
void skype_update_buddy_icon(PurpleBuddy *buddy);
static void skype_send_file_from_blist(PurpleBlistNode *node, gpointer data);
static GList *skype_node_menu(PurpleBlistNode *node);
static void skype_call_user_from_blist(PurpleBlistNode *node, gpointer data);
static void skype_silence(PurplePlugin *plugin, gpointer data);
void skype_slist_friend_check(gpointer buddy_pointer, gpointer friends_pointer);
int skype_slist_friend_search(gconstpointer buddy_pointer, gconstpointer buddyname_pointer);
static int skype_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags);
static void skype_chat_leave(PurpleConnection *gc, int id);
static void skype_chat_invite(PurpleConnection *gc, int id, const char *msg, const char *who);
static void skype_initiate_chat(PurpleBlistNode *node, gpointer data);
static void skype_set_chat_topic(PurpleConnection *gc, int id, const char *topic);
void skype_alias_buddy(PurpleConnection *gc, const char *who, const char *alias);
gboolean skype_offline_msg(const PurpleBuddy *buddy);
void skype_slist_remove_messages(gpointer buddy_pointer, gpointer unused);
static void skype_program_update_check(void);
static void skype_plugin_update_check(void);
void skype_plugin_update_callback(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);
void skype_program_update_callback(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);
gchar *timestamp_to_datetime(time_t timestamp);
void skype_show_search_users(PurplePluginAction *action);
static void skype_search_users(PurpleConnection *gc, const gchar *searchterm);
void skype_searchresults_add_buddy(PurpleConnection *gc, GList *row, void *user_data);

#ifndef G_GNUC_NULL_TERMINATED
#  if __GNUC__ >= 4
#    define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#  else
#    define G_GNUC_NULL_TERMINATED
#  endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

PurplePluginProtocolInfo prpl_info = {
	/* options */
	  OPT_PROTO_NO_PASSWORD|OPT_PROTO_REGISTER_NOSCREENNAME|
	  OPT_PROTO_UNIQUE_CHATNAME|OPT_PROTO_CHAT_TOPIC,

	NULL,                /* user_splits */
	NULL,                /* protocol_options */
	{"png,gif,jpeg", 0, 0, 96, 96, 65535, PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
	skype_list_icon,     /* list_icon */
	NULL,                /* list_emblems */
	skype_status_text,   /* status_text */
	skype_tooltip_text,  /* tooltip_text */
	skype_status_types,  /* status_types */
	skype_node_menu,     /* blist_node_menu */
	NULL,                /* chat_info */
	NULL,                /* chat_info_defaults */
	skype_login,         /* login */
	skype_close,         /* close */
	skype_send_im,       /* send_im */
	NULL,                /* set_info */
	NULL,                /* send_typing */
	skype_get_info,      /* get_info */
	skype_set_status,    /* set_status */
	NULL/*skype_set_idle*/,      /* set_idle */
	NULL,                /* change_passwd */
	skype_add_buddy,     /* add_buddy */
	NULL,                /* add_buddies */
	skype_remove_buddy,  /* remove_buddy */
	NULL,                /* remove_buddies */
	skype_add_permit,    /* add_permit */
	skype_add_deny,      /* add_deny */
	skype_rem_permit,    /* rem_permit */
	skype_rem_deny,      /* rem_deny */
	NULL,                /* set_permit_deny */
	NULL,                /* join_chat */
	NULL,                /* reject chat invite */
	NULL,                /* get_chat_name */
	skype_chat_invite,   /* chat_invite */
	skype_chat_leave,    /* chat_leave */
	NULL,                /* chat_whisper */
	skype_chat_send,     /* chat_send */
	skype_keepalive,     /* keepalive */
	NULL,                /* register_user */
	NULL,                /* get_cb_info */
	NULL,                /* get_cb_away */
	skype_alias_buddy,   /* alias_buddy */
	NULL,                /* group_buddy */
	NULL,                /* rename_group */
	NULL,                /* buddy_free */
	NULL,                /* convo_closed */
	skype_normalize,     /* normalize */
	skype_set_buddy_icon,/* set_buddy_icon */
	NULL,                /* remove_group */
	NULL,                /* get_cb_real_name */
	skype_set_chat_topic,/* set_chat_topic */
	NULL,                /* find_blist_chat */
	NULL,                /* roomlist_get_list */
	NULL,                /* roomlist_cancel */
	NULL,                /* roomlist_expand_category */
	NULL,                /* can_receive_file */
	NULL,                /* send_file */
	NULL,                /* new_xfer */
	skype_offline_msg,   /* offline_message */
	NULL,                /* whiteboard_prpl_ops */
	NULL,                /* send_raw */
	NULL,                /* roomlist_room_serialize */
	NULL,                /* _purple_reserved1 */
	NULL,                /* _purple_reserved2 */
	NULL,                /* _purple_reserved3 */
	NULL                 /* _purple_reserved4 */
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,
	NULL,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"prpl-bigbrownchunx-skype",
	"Skype",
	"1.2",
	"Allows using Skype IM functions from within Pidgin",
	"Allows using Skype IM functions from within Pidgin",
	"Eion Robb <eion@robbmob.com>",
	"http://tinyurl.com/2by8rw",
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	&prpl_info,
	NULL, //prefs_info
	skype_actions,
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePlugin *this_plugin;

static void
plugin_init(PurplePlugin *plugin)
{
	PurpleAccountOption *option;

	if (!g_thread_supported ())
		g_thread_init (NULL);
	this_plugin = plugin;
	/* plugin's path at
		this_plugin->path */
/*
#ifdef __APPLE__
	//Adium demands a server and port, but there isn't one
	option = purple_account_option_string_new(_("Server"), "server", "localhost");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_int_new(_("Port"), "port", 0);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#endif
*/
	option = purple_account_option_bool_new(_("Show SkypeOut contacts as 'Online'"), "skypeout_online", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_bool_new(_("Make Skype online/offline when going online/offline"), "skype_sync", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_bool_new(_("Automatically check for updates"), "check_for_updates", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

PURPLE_INIT_PLUGIN(skype, plugin_init, info);



static PurpleAccount *
skype_get_account(PurpleAccount *newaccount)
{
	static PurpleAccount* account;
	if (newaccount != NULL)
		account = newaccount;
	return account;
}

gboolean
plugin_load(PurplePlugin *plugin)
{
	return TRUE;
}

gboolean
plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static GList *
skype_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		act = purple_menu_action_new(_("Send File..."),
										PURPLE_CALLBACK(skype_send_file_from_blist),
										NULL, NULL);
		m = g_list_append(m, act);
		
		act = purple_menu_action_new(_("Call..."),
										PURPLE_CALLBACK(skype_call_user_from_blist),
										NULL, NULL);
		m = g_list_append(m, act);

		act = purple_menu_action_new(_("Initiate Chat"),
							PURPLE_CALLBACK(skype_initiate_chat),
							NULL, NULL);
		m = g_list_append(m, act);
	}
	return m;
}

static void
skype_send_file_from_blist(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;

	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		buddy = (PurpleBuddy *) node;
		if (PURPLE_BUDDY_IS_ONLINE(buddy))
		{
			skype_send_message_nowait("OPEN FILETRANSFER %s", buddy->name);
		}
	}
}

static void
skype_call_user_from_blist(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		buddy = (PurpleBuddy *) node;
		if (PURPLE_BUDDY_IS_ONLINE(buddy))
		{
			skype_send_message_nowait("CALL %s", buddy->name);
		}
	}
}

const char *
skype_normalize(const PurpleAccount *acct, const char *who)
{
	return g_utf8_strdown(who, -1);
}

GList *
skype_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	PurpleMenuAction *act;

	act = purple_menu_action_new(_("Hide Skype"),
									PURPLE_CALLBACK(skype_silence),
									NULL, NULL);
	m = g_list_append(m, act);

	act = purple_menu_action_new(_("Check for Skype updates"),
									PURPLE_CALLBACK(skype_program_update_check),
									NULL, NULL);
	m = g_list_append(m, act);

	act = purple_menu_action_new(_("Check for plugin updates"),
									PURPLE_CALLBACK(skype_plugin_update_check),
									NULL, NULL);
	m = g_list_append(m, act);

	act = purple_menu_action_new(_("Search for buddies"),
									PURPLE_CALLBACK(skype_show_search_users),
									NULL, NULL);
	m = g_list_append(m, act);
	return m;
}

static void
skype_silence(PurplePlugin *plugin, gpointer data)
{
	skype_send_message_nowait("SET SILENT_MODE ON");
	skype_send_message_nowait("MINIMIZE");
	hide_skype();
}

static void
skype_plugin_update_check()
{
	gchar *filename, *basename;
	struct stat *filestat = g_new(struct stat, 1);

	//this_plugin is the PidginPlugin
	if (this_plugin == NULL)
		return;

	filename = this_plugin->path;
	if (g_stat(filename, filestat) == -1)
		return;
	
	basename = g_path_get_basename(filename);
	purple_util_fetch_url(g_strconcat("http://myjob", "space.co.nz/images/pidgin/", "?version=", basename, NULL),
		TRUE, NULL, FALSE, skype_plugin_update_callback, (gpointer)filestat);
	
}

void
skype_plugin_update_callback(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	time_t mtime = ((struct stat *) user_data)->st_mtime;
	time_t servertime = atoi(url_text);
	purple_debug_info("skype", "Server filemtime: %d, Local filemtime: %d\n", servertime, mtime);
	if (servertime > mtime)
	{
		purple_notify_info(this_plugin, "Update available", "There is a newer version of the Skype plugin available for download.", 
				g_strconcat("Your version: ",timestamp_to_datetime(mtime),"\nLatest version: ",timestamp_to_datetime(servertime),"\nLatest version available from: ", this_plugin->info->homepage, NULL));
	} else {
		purple_notify_info(this_plugin, "No Updates", "No updates found", "You have the latest version of the Skype plugin");
	}
}

static void
skype_program_update_check()
{
	/*
		Windows:
		http://ui.skype.com/ui/0/3.5.0.239/en/getnewestversion

		Linux:
		http://ui.skype.com/ui/2/2.0.0.13/en/getnewestversion

		Mac:
		http://ui.skype.com/ui/3/2.6.0.151/en/getnewestversion

		User-Agent: Skype
	*/

	gchar *version;
	gchar *temp;
	gchar version_url[60];
	int platform_number;

#ifdef _WIN32
	platform_number = 0;
#else
#	ifdef __APPLE__
	platform_number = 3;
#	else
	platform_number = 2;
#	endif
#endif

	temp = skype_send_message("GET SKYPEVERSION");
	version = g_strdup(&temp[13]);
	g_free(temp);

	sprintf(version_url, "http://ui.skype.com/ui/%d/%s/en/getnewestversion", platform_number, version);
	purple_util_fetch_url(version_url, TRUE, "Skype", TRUE, skype_program_update_callback, (gpointer)version);
}

void
skype_program_update_callback(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	gchar *version = (gchar *)user_data;
	int v1, v2, v3, v4;
	int s1, s2, s3, s4;
	gboolean newer_version = FALSE;
	
	sscanf(version, "%d.%d.%d.%d", &v1, &v2, &v3, &v4);
	sscanf(url_text, "%d.%d.%d.%d", &s1, &s2, &s3, &s4);

	if (s1 > v1)
		newer_version = TRUE;
	else if (s1 == v1 && s2 > v2)
		newer_version = TRUE;
	else if (s1 == v1 && s2 == v2 && s3 > v3)
		newer_version = TRUE;
	else if (s1 == v1 && s2 == v2 && s3 == v3 && s4 > v4)
		newer_version = TRUE;

	if (newer_version)
	{
		purple_notify_info(this_plugin, "Update available", "There is a newer version of Skype available for download.", g_strconcat("Your version: ", version, "\nLatest version: ", url_text, "\n\nhttp://www.skype.com/go/download", NULL));
	} else {
		purple_notify_info(this_plugin, "No Updates", "No updates found", "You have the latest version of Skype");
	}
}

GList *
skype_status_types(PurpleAccount *acct)
{
	GList *types;
	PurpleStatusType *status;

	purple_debug_info("skype", "returning status types\n");

	types = NULL;

    /* Statuses are almost all the same. Define a macro to reduce code repetition. */
#define _SKYPE_ADD_NEW_STATUS(prim,name) status =                    \
	purple_status_type_new_with_attrs(                          \
	prim,   /* PurpleStatusPrimitive */                         \
	NULL,   /* id - use default */                              \
	_(name),/* name */                            \
	TRUE,   /* savable */                                       \
	TRUE,   /* user_settable */                                 \
	FALSE,  /* not independent */                               \
	                                                            \
	/* Attributes - each status can have a message. */          \
	"message",                                                  \
	_("Message"),                                               \
	purple_value_new(PURPLE_TYPE_STRING),                       \
	NULL);                                                      \
	                                                            \
	                                                            \
	types = g_list_append(types, status)


	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_AVAILABLE, "Online");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_AWAY, "Away");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_EXTENDED_AWAY, "Not Available");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_UNAVAILABLE, "Do Not Disturb");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_INVISIBLE, "Invisible");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_OFFLINE, "Offline");


	return types;
}

gboolean
skype_set_buddies(PurpleAccount *acct)
{
	char *friends_text;
	char **friends;
	GSList *existing_friends;
	GSList *found_buddy;
	PurpleBuddy *buddy;
	PurpleGroup *skype_group;
	int i;

	skype_group = purple_group_new("Skype");
	purple_blist_add_group(skype_group, NULL);


	friends_text = skype_send_message("SEARCH FRIENDS");
	friends = g_strsplit_set(friends_text, ", ", 0);
	if (friends == NULL || friends[0] == NULL)
	{
		return FALSE;
	}
	
	//Remove from libpurple buddy list if not in skype friends list
	existing_friends = purple_find_buddies(acct, NULL);
	g_slist_foreach(existing_friends, (GFunc)skype_slist_friend_check, friends);
	
	//grab the list of buddy's again since they could have changed
	existing_friends = purple_find_buddies(acct, NULL);

	for (i=1; friends[i]; i++)
	{
		if (strlen(friends[i]) == 0)
			continue;
		//If already in list, dont recreate, reuse
		purple_debug_info("skype", "Searching for friend %s\n", friends[i]);
		found_buddy = g_slist_find_custom(existing_friends,
											friends[i],
											skype_slist_friend_search);
		if (found_buddy != NULL)
		{
			//the buddy was already in the list
			buddy = (PurpleBuddy *)found_buddy->data;
			purple_debug_info("skype","Buddy already in list: %s (%s)\n", buddy->name, friends[i]);
		} else {
			purple_debug_info("skype","Buddy not in list %s\n", friends[i]);
			buddy = purple_buddy_new(acct, g_strdup(friends[i]), NULL);
			purple_blist_add_buddy(buddy, NULL, skype_group, NULL);
		}
		skype_update_buddy_status(buddy);
		skype_update_buddy_alias(buddy);
		purple_prpl_got_user_idle(acct, buddy->name, FALSE, 0);

		skype_update_buddy_icon(buddy);
	}
	
	//special case, if we're on our own buddy list
	if ((found_buddy = g_slist_find_custom(existing_friends, skype_get_account_username(acct), skype_slist_friend_search)))
	{
		buddy = (PurpleBuddy *)found_buddy->data;
		skype_update_buddy_status(buddy);
		skype_update_buddy_alias(buddy);
		purple_prpl_got_user_idle(acct, buddy->name, FALSE, 0);
		skype_update_buddy_icon(buddy);
	}

	purple_debug_info("skype", "Friends Count: %d\n", i);
	g_strfreev(friends);
	
	return FALSE;
}

void
skype_slist_friend_check(gpointer buddy_pointer, gpointer friends_pointer)
{
	int i;
	PurpleBuddy *buddy = (PurpleBuddy *)buddy_pointer;
	char **friends = (char **)friends_pointer;

	if (strcmp(buddy->name, skype_get_account_username(NULL)) == 0)
	{
		//we must have put ourselves on our own list in pidgin, ignore
		return;
	}

	for(i=1; friends[i]; i++)
	{
		if (strlen(friends[i]) == 0)
			continue;
		if (strcmp(buddy->name, friends[i]) == 0)
			return;
	}
	purple_debug_info("skype", "removing buddy %d with name %s\n", buddy, buddy->name);
	purple_blist_remove_buddy(buddy);
}

int
skype_slist_friend_search(gconstpointer buddy_pointer, gconstpointer buddyname_pointer)
{
	PurpleBuddy *buddy;
	gchar *buddyname;

	if (buddy_pointer == NULL)
		return -1;
	if (buddyname_pointer == NULL)
		return 1;

	buddy = (PurpleBuddy *)buddy_pointer;
	buddyname = (gchar *)buddyname_pointer;

	if (buddy->name == NULL)
		return -1;
	
	return strcmp(buddy->name, buddyname);
}

void
skype_update_buddy_icon(PurpleBuddy *buddy)
{
	PurpleAccount *acct;
	gchar *filename = NULL;
	gchar *new_filename = NULL;
	gchar *image_data = NULL;
	gsize image_data_len = 0;
	gchar *ret;
	int fh;
	GError *error;
	static gboolean api_supports_avatar = TRUE;
	
	if (api_supports_avatar == FALSE)
	{
		return;
	}
	
	purple_debug_info("skype", "Updating buddy icon for %s\n", buddy->name);

	acct = purple_buddy_get_account(buddy);
	fh = g_file_open_tmp("skypeXXXXXX", &filename, &error);
	close(fh);
	
	if (filename != NULL)
	{
		new_filename = g_strconcat(filename, ".jpg", NULL);
		g_rename(filename, new_filename);
		ret = skype_send_message("GET USER %s AVATAR 1 %s", buddy->name, new_filename);
		if (strlen(ret) == 0)
		{
			purple_debug_info("skype", "Error: Protocol doesn't suppot AVATAR\n");
			api_supports_avatar = FALSE;
		} else {
			g_file_get_contents(new_filename, &image_data, &image_data_len, NULL);
		}
		g_unlink(new_filename);
		g_free(filename);
		g_free(new_filename);
	} else {
		purple_debug_info("skype", "Error making temp file %s\n", error->message);
		g_error_free(error);
	}

	purple_buddy_icons_set_for_user(acct, buddy->name, g_memdup(image_data,image_data_len), image_data_len, NULL);
}


void
skype_update_buddy_alias(PurpleBuddy *buddy)
{
	char *alias;
	
	alias = skype_get_user_info(buddy->name, "DISPLAYNAME");
	if (strlen(alias) > 0)
	{
		purple_blist_server_alias_buddy(buddy, alias);
		return;
	}
	
	alias = skype_get_user_info(buddy->name, "FULLNAME");
	if (strlen(alias) > 0)
	{
		purple_blist_server_alias_buddy(buddy, alias);
	}
}

gboolean
skype_update_buddy_status(PurpleBuddy *buddy)
{
	char *status;
	PurpleStatusPrimitive primitive;
	PurpleAccount *acct;
	
	acct = purple_buddy_get_account(buddy);
	status = skype_get_user_info(buddy->name, "ONLINESTATUS");

	purple_debug_info("skype", "User %s status is %s\n", buddy->name, status);
	
	if (strcmp(status, "OFFLINE") == 0)
	{
		primitive = PURPLE_STATUS_OFFLINE;
		if (strcmp(skype_get_user_info(buddy->name, "IS_VOICEMAIL_CAPABLE"), "TRUE") == 0)
		{
			buddy->proto_data = g_strdup(_("Offline with Voicemail"));
		} else if (strcmp(skype_get_user_info(buddy->name, "IS_CF_ACTIVE"), "TRUE") == 0)
		{
			buddy->proto_data = g_strdup(_("Offline with Call Forwarding"));
		}
	} else if (strcmp(status, "ONLINE") == 0 ||
			strcmp(status, "SKYPEME") == 0)
	{
		primitive = PURPLE_STATUS_AVAILABLE;
	} else if (strcmp(status, "AWAY") == 0)
	{
		primitive = PURPLE_STATUS_AWAY;
	} else if (strcmp(status, "NA") == 0)
	{
		primitive = PURPLE_STATUS_EXTENDED_AWAY;
	} else if (strcmp(status, "DND") == 0)
	{
		primitive = PURPLE_STATUS_UNAVAILABLE;
	} else if (strcmp(status, "SKYPEOUT") == 0)
	{
		if (purple_account_get_bool(buddy->account, "skypeout_online", TRUE))
		{
			primitive = PURPLE_STATUS_AVAILABLE;
		} else {
			primitive = PURPLE_STATUS_OFFLINE;
		}
		buddy->proto_data = g_strdup(_("SkypeOut"));
	} else
	{
		primitive = PURPLE_STATUS_UNSET;
	}
	purple_prpl_got_user_status(acct, buddy->name, purple_primitive_get_id_from_type(primitive), NULL);
	
	/* if this function was called from another thread, don't loop over it */
	return FALSE;
}

void 
skype_login(PurpleAccount *acct)
{
	PurpleConnection *gc;
	const gchar *reply;
	gboolean connect_successful;
	
	if(acct == NULL)
	{
		return;
	}

	gc = purple_account_get_connection(acct);
	gc->flags = PURPLE_CONNECTION_NO_BGCOLOR |
				PURPLE_CONNECTION_NO_URLDESC |
				PURPLE_CONNECTION_NO_FONTSIZE |
				PURPLE_CONNECTION_NO_IMAGES;

	/* 1. connect to server */
	/* TODO: Work out if a skype connection is already running */
	if(FALSE)
	{
		purple_connection_error(gc, "\nOnly one Skype account allowed");
		return;
	}
	
	
	purple_connection_update_progress(gc, _("Connecting"), 0, 5);

	connect_successful = skype_connect();
	if (!connect_successful)
	{
		purple_connection_error(gc, "\nCould not connect to Skype process\nSkype not running?");
		return;
	}
	
	purple_connection_update_progress(gc, _("Authorizing"),
								  1,   /* which connection step this is */
								  5);  /* total number of steps */

#ifndef __APPLE__
	reply = skype_send_message("NAME Pidgin");
	if (reply == NULL || strlen(reply) == 0)
	{
		purple_connection_error(gc, "\nSkype client not ready");
		return;
	}
#endif
	purple_connection_update_progress(gc, _("Initializing"), 1, 5);
	reply = skype_send_message("PROTOCOL 2");
	if (reply == NULL || strlen(reply) == 0)
	{
		purple_connection_error(gc, "\nSkype client not ready");
	}
	
	purple_connection_update_progress(gc, _("Silencing Skype"), 2, 5);
	skype_silence(NULL, NULL);
	purple_connection_set_state(gc, PURPLE_CONNECTED);

	skype_get_account(acct);
	skype_get_account_alias(acct);
	skype_get_account_username(acct);
	if (purple_account_get_bool(acct, "skype_sync", TRUE))
		skype_set_status(acct, purple_account_get_active_status(acct));
	//sync buddies after everything else has finished loading
	purple_timeout_add(10, (GSourceFunc)skype_set_buddies, (gpointer)acct);		
}

char *
skype_get_account_username(PurpleAccount *acct)
{
	char *ret;
	static char *username = NULL;
	
	if (username != NULL)
		return username;
	
	ret = skype_send_message("GET CURRENTUSERHANDLE");
	username = g_strdup(&ret[18]);
	g_free(ret);

	if (acct && strcmp(acct->username, username) != 0)
	{
		purple_debug_info("skype", "Setting username to %s\n", username);
		purple_account_set_username(acct, username);
	}
	return username;
}

void
skype_get_account_alias(PurpleAccount *acct)
{
	char *ret;
	char *alias;
	ret = skype_send_message("GET PROFILE FULLNAME");
	alias = g_strdup(&ret[17]);
	g_free(ret);
	purple_account_set_alias(acct, alias);
}

void
skype_slist_remove_messages(gpointer buddy_pointer, gpointer unused)
{
	PurpleBuddy *buddy = (PurpleBuddy *)buddy_pointer;
	if (buddy && buddy->proto_data)
	{
		buddy->proto_data = NULL;
	}
}

void 
skype_close(PurpleConnection *gc)
{
	GSList *buddies;

	purple_debug_info("skype", "logging out\n");
	if (purple_account_get_bool(gc->account, "skype_sync", TRUE))
		skype_send_message("SET USERSTATUS OFFLINE");
	skype_send_message_nowait("SET SILENT_MODE OFF");
	skype_disconnect();
	buddies = purple_find_buddies(gc->account, NULL);
	if (buddies != NULL && g_slist_length(buddies) > 0)
		g_slist_foreach(buddies, skype_slist_remove_messages, NULL);
}

int 
skype_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, 
		PurpleMessageFlags flags)
{
	char *stripped;
	
	stripped = purple_markup_strip_html(message);
	
	skype_send_message_nowait("MESSAGE %s %s", who, stripped);
	
	return 1;
}

gchar *
timestamp_to_datetime(time_t timestamp)
{
	return g_strdup(purple_date_format_long(localtime(&timestamp)));
}

void 
skype_get_info(PurpleConnection *gc, const gchar *username)
{
	PurpleNotifyUserInfo *user_info;
	double timezoneoffset;
	char timezone_str[9];
	int time;
	
	user_info = purple_notify_user_info_new();
#define _SKYPE_USER_INFO(prop, key)  \
	purple_notify_user_info_add_pair(user_info, _(key),\
		g_strdup(skype_get_user_info(username, prop)));

	purple_notify_user_info_add_section_header(user_info, _("Contact Information"));
	_SKYPE_USER_INFO("HANDLE", "Handle");
	_SKYPE_USER_INFO("FULLNAME", "Full Name");
	purple_notify_user_info_add_section_break(user_info);
	purple_notify_user_info_add_section_header(user_info, _("Personal Information"));
	_SKYPE_USER_INFO("BIRTHDAY", "Birthday");
	_SKYPE_USER_INFO("SEX", "Gender");
	_SKYPE_USER_INFO("LANGUAGE", "Language");
	_SKYPE_USER_INFO("COUNTRY", "Country");
	_SKYPE_USER_INFO("ABOUT", "About");
	_SKYPE_USER_INFO("IS_VIDEO_CAPABLE", "Is Video Capable");
	_SKYPE_USER_INFO("ISAUTHORIZED", "Authorised");
	_SKYPE_USER_INFO("ISBLOCKED", "Blocked");
	//_SKYPE_USER_INFO("LASTONLINETIMESTAMP", "Last Online"); //timestamp
	time = atoi(skype_get_user_info(username, "LASTONLINETIMESTAMP"));
	purple_debug_info("skype", "time: %d\n", time);
	purple_notify_user_info_add_pair(user_info, _("Last Online"),
			timestamp_to_datetime((time_t) time));
	//		g_strdup(purple_date_format_long(localtime((time_t *)(void *)&time))));
	//_SKYPE_USER_INFO("TIMEZONE", "Timezone"); //in seconds
	timezoneoffset = atof(skype_get_user_info(username, "TIMEZONE")) / 3600;
	g_snprintf(timezone_str, 9, "UMT +%.1f", timezoneoffset);
	purple_notify_user_info_add_pair(user_info, _("Timezone"), g_strdup(timezone_str));
	
	_SKYPE_USER_INFO("NROF_AUTHED_BUDDIES", "Number of buddies");
	purple_notify_user_info_add_section_break(user_info);
	_SKYPE_USER_INFO("MOOD_TEXT", "Mood");

	
	purple_notify_userinfo(gc, username, user_info, NULL, NULL);
	purple_notify_user_info_destroy(user_info);
}

gchar *
skype_get_user_info(const gchar *username, const gchar *property)
{
	gchar *outstr;
	gchar *return_str;
	outstr = skype_send_message("GET USER %s %s", username, property);
	return_str = g_strdup(&outstr[7+strlen(username)+strlen(property)]);
	g_free(outstr);
	/* purple_debug_info("skype", "User %s's %s is %s", username, property, return_str); */
	if (return_str == NULL)
		return NULL;
	return return_str;
}

void
skype_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleStatusType *type;
	const char *message;
	
	type = purple_status_get_type(status);
	switch (purple_status_type_get_primitive(type)) {
		case PURPLE_STATUS_AVAILABLE:
			skype_send_message_nowait("SET USERSTATUS ONLINE");
			break;
		case PURPLE_STATUS_AWAY:
			skype_send_message_nowait("SET USERSTATUS AWAY");
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			skype_send_message_nowait("SET USERSTATUS NA");
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			skype_send_message_nowait("SET USERSTATUS DND");
			break;
		case PURPLE_STATUS_INVISIBLE:
			skype_send_message_nowait("SET USERSTATUS INVISIBLE");
			break;
		case PURPLE_STATUS_OFFLINE:
			skype_send_message_nowait("SET USERSTATUS OFFLINE");
			break;
		default:
			skype_send_message_nowait("SET USERSTATUS UNKNOWN");
	}
	
	message = purple_status_get_attr_string(status, "message");
	if (message == NULL)
		message = "";
	skype_send_message_nowait("SET PROFILE RICH_MOOD_TEXT <!-- libpurple:%s --> %s",
						purple_core_get_ui(), message);
}

void
skype_set_idle(PurpleConnection *gc, int time)
{
	if (time == 0) {
		skype_send_message_nowait("SET USERSTATUS ONLINE");
	} else {
		skype_send_message_nowait("SET USERSTATUS AWAY");
	}
}


void 
skype_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	if (strcmp(skype_get_user_info(buddy->name, "ONLINESTATUS"), "UNKNOWN") == 0)
	{
		//this user doesn't exist
		purple_blist_remove_buddy(buddy);
		purple_notify_error(gc, "Error", "User does not exist", "The user does not exist in Skype");
		return;
	}

	skype_send_message_nowait("SET USER %s BUDDYSTATUS 2 Please authorise me", buddy->name);
	if (buddy->alias == NULL || strlen(buddy->alias) == 0)
		skype_update_buddy_alias(buddy);
	if (group && group->name)
	{
		/* TODO: Deal with groups */
	}
	skype_add_permit(gc, buddy->name);
	skype_rem_deny(gc, buddy->name);
}

void 
skype_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	skype_send_message_nowait("SET USER %s BUDDYSTATUS 1", buddy->name);
}

void
skype_add_deny(PurpleConnection *gc, const char *who)
{
	skype_send_message_nowait("SET USER %s ISBLOCKED TRUE", who);
}

void
skype_rem_deny(PurpleConnection *gc, const char *who)
{
	skype_send_message_nowait("SET USER %s ISBLOCKED FALSE", who);
}

void
skype_add_permit(PurpleConnection *gc, const char *who)
{
	skype_send_message_nowait("SET USER %s ISAUTHORIZED TRUE", who);
}

void
skype_rem_permit(PurpleConnection *gc, const char *who)
{
	skype_send_message_nowait("SET USER %s ISAUTHORIZED FALSE", who);
}

void
skype_keepalive(PurpleConnection *gc)
{
	skype_send_message_nowait("PING");
}

void
skype_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img)
{
	skype_send_message_nowait("SET AVATAR 1 %s:1", purple_imgstore_get_filename(img));
}

char *
skype_status_text(PurpleBuddy *buddy)
{
	char *mood_text;
	PurplePresence *presence;
	PurpleStatus *status;
	PurpleStatusType *type;

	if (buddy->proto_data != NULL && strlen(buddy->proto_data))
		return g_strdup((char *)buddy->proto_data);
		
	if (buddy->proto_data == NULL)
	{
		mood_text = skype_get_user_info(buddy->name, "MOOD_TEXT");
		buddy->proto_data = mood_text;
		if (mood_text != NULL && strlen(mood_text))
			return g_strdup(mood_text);
	}

	//If we're at this point, they don't have a mood.
	//Use away status instead
	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);
	type = purple_status_get_type(status);
	if (purple_status_type_get_primitive(type) == PURPLE_STATUS_AVAILABLE)
		return NULL;
	mood_text = (char *)purple_status_type_get_name(type);
	if (mood_text != NULL && strlen(mood_text))
		return g_strdup(mood_text);

	return NULL;
}

void
skype_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *userinfo, gboolean full)
{
	PurplePresence *presence;
	PurpleStatus *status;

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);
	purple_notify_user_info_add_pair(userinfo, _("Status"), purple_status_get_name(status));
	if (buddy->proto_data && strlen(buddy->proto_data))
		purple_notify_user_info_add_pair(userinfo, _("Message"), buddy->proto_data);
}

const char *
skype_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "skype";
}

static void
skype_initiate_chat(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConversation *conv;
	gchar *msg;
	gchar chat_id[200];

	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		buddy = (PurpleBuddy *) node;
		if (PURPLE_BUDDY_IS_ONLINE(buddy))
		{
			msg = skype_send_message("CHAT CREATE %s", buddy->name);
			sscanf(msg, "CHAT %s ", chat_id);
		}
		conv = purple_conversation_new(PURPLE_CONV_TYPE_CHAT, buddy->account, buddy->name);
		purple_debug_info("skype", "Conv Hash Table: %d\n", conv->data);
		purple_debug_info("skype", "chat_id: %s\n", chat_id);
		g_hash_table_insert(conv->data, "chat_id", g_strdup(chat_id));
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),
									purple_account_get_username(buddy->account), NULL, PURPLE_CBFLAGS_NONE, FALSE);
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),
									buddy->name, NULL, PURPLE_CBFLAGS_NONE, FALSE);
	}
}

static void
skype_chat_invite(PurpleConnection *gc, int id, const char *msg, const char *who)
{
	PurpleConversation *conv;
	gchar *chat_id;

	conv = purple_find_chat(gc, id);
	chat_id = (gchar *)g_hash_table_lookup(conv->data, "chat_id");

	skype_send_message_nowait("ALTER CHAT %s ADDMEMBERS %s", chat_id, who);
}

static void
skype_chat_leave(PurpleConnection *gc, int id)
{
	PurpleConversation *conv;
	gchar* chat_id;

	conv = purple_find_chat(gc, id);
	chat_id = (gchar *)g_hash_table_lookup(conv->data, "chat_id");

	skype_send_message_nowait("ALTER CHAT %s LEAVE", chat_id);
}

static int
skype_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
	
	PurpleConversation *conv;
	gchar* chat_id;

	conv = purple_find_chat(gc, id);
	chat_id = (gchar *)g_hash_table_lookup(conv->data, "chat_id");

	skype_send_message_nowait("CHATMESSAGE %s %s", chat_id, message);

	serv_got_chat_in(gc, id, purple_account_get_username(purple_connection_get_account(gc)), 0,
					message, time(NULL));

	return 1;
}

static void
skype_set_chat_topic(PurpleConnection *gc, int id, const char *topic)
{
	PurpleConversation *conv;
	gchar* chat_id;

	conv = purple_find_chat(gc, id);
	chat_id = (gchar *)g_hash_table_lookup(conv->data, "chat_id");

	skype_send_message_nowait("ALTER CHAT %s SETTOPIC %s", chat_id, topic);
}

void
skype_alias_buddy(PurpleConnection *gc, const char *who, const char *alias)
{
	skype_send_message_nowait("SET USER %s DISPLAYNAME %s", who, alias);
}

gboolean
skype_offline_msg(const PurpleBuddy *buddy)
{
	return TRUE;
}

void
skype_show_search_users(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	purple_request_input(gc, _("Search for Skype Users"),
					   _("Search for Skype Users"),
					   _("Type the Skype Name, full name or e-mail address of the buddy you are "
						 "searching for."),
					   NULL, FALSE, FALSE, NULL,
					   _("_Search"), G_CALLBACK(skype_search_users),
					   _("_Cancel"), NULL,
					   purple_connection_get_account(gc), NULL, NULL,
					   gc);

}

static void
skype_search_users(PurpleConnection *gc, const gchar *searchterm)
{
	PurpleNotifySearchResults *results;
	PurpleNotifySearchColumn *column;
	gchar *userlist;
	gchar **list_of_users;
	int i = 0;
	
	results = purple_notify_searchresults_new();
	if (results == NULL)
		return;

	//columns: Full Name, Skype Name, Country/Region, Profile Link
	column = purple_notify_searchresults_column_new(_("Full Name"));
	purple_notify_searchresults_column_add(results, column);
	column = purple_notify_searchresults_column_new(_("Skype Name"));
	purple_notify_searchresults_column_add(results, column);
	column = purple_notify_searchresults_column_new(_("Country/Region"));
	purple_notify_searchresults_column_add(results, column);
	
	//buttons: Add Skype Contact, Close
	purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_ADD, 
										skype_searchresults_add_buddy);

	userlist = skype_send_message("SEARCH USERS %s", searchterm);
	list_of_users = g_strsplit(&userlist[6], ", ", -1);
	while(list_of_users[i])
	{
		GList *row = NULL;
		row = g_list_append(row, skype_get_user_info(list_of_users[i], "FULLNAME"));
		row = g_list_append(row, g_strdup(list_of_users[i]));
		row = g_list_append(row, g_strconcat(skype_get_user_info(list_of_users[i], "CITY"), ", ", skype_get_user_info(list_of_users[i], "COUNTRY"), NULL));
		purple_notify_searchresults_row_add(results, row);
		i++;
	}
	g_strfreev(list_of_users);
	g_free(userlist);
	
	purple_notify_searchresults(gc, NULL, NULL, NULL, results, NULL, NULL);
}

void
skype_searchresults_add_buddy(PurpleConnection *gc, GList *row, void *user_data)
{
	purple_blist_request_add_buddy(purple_connection_get_account(gc),
								 g_list_nth_data(row, 1), NULL, NULL);
}
