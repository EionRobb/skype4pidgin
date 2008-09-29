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
#define PURPLE_PLUGINS
#define DBUS_API_SUBJECT_TO_CHANGE
#define _GNU_SOURCE
#define GETTEXT_PACKAGE "skype4pidgin"

#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <string.h>

//#include <internal.h>

#ifdef ENABLE_NLS
#	ifdef _WIN32
#		include <win32dep.h>
#	endif
#	include <glib/gi18n-lib.h>
#else
#	define _(a) a
#endif

#ifndef G_GNUC_NULL_TERMINATED
#  if __GNUC__ >= 4
#    define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#  else
#    define G_GNUC_NULL_TERMINATED
#  endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#include <notify.h>
#include <core.h>
#include <plugin.h>
#include <version.h>
#include <accountopt.h>
#include <blist.h>
#include <request.h>
#include <cmds.h>
#include <status.h>

#ifdef USE_FARSIGHT
#include <mediamanager.h>
PurpleMedia *skype_media_initiate(PurpleConnection *gc, const char *who, PurpleMediaStreamType type);
#endif

typedef struct _SkypeBuddy {
	PurpleBuddy *buddy;
	
	//this bit used in Get Info
	gchar *handle;
	gchar *fullname;
	gchar *mood;
	struct tm *birthday;
	gchar *gender;
	gchar *language;
	gchar *country;
	gboolean is_video_capable;
	gboolean is_authorized;
	gboolean is_blocked;
	time_t last_online;
	gdouble timezone_offset;
	guint number_of_buddies;
	gchar *about;
	
	//this bit not used yet
	gchar *province;
	gchar *city;
	gchar *phone_home;
	gchar *phone_office;
	gchar *phone_mobile;
	gchar *homepage;
	gboolean has_call_equipment;
	gboolean is_voicemail_capable;
	gboolean is_callforward_active;
	gboolean can_leave_voicemail;
	
	//libpurple buddy stuff
	guint typing_stream;
} SkypeBuddy;

#include "debug.c"
#include "skype_messaging.c"

static void plugin_init(PurplePlugin *plugin);
gboolean plugin_load(PurplePlugin *plugin);
gboolean plugin_unload(PurplePlugin *plugin);
GList *skype_status_types(PurpleAccount *acct);
void skype_login(PurpleAccount *acct);
void skype_close(PurpleConnection *gc);
int skype_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags);
int skype_send_sms(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags);
static void hide_skype();
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
const char *skype_get_account_username(PurpleAccount *acct);
static PurpleAccount *skype_get_account(PurpleAccount *newaccount);
void skype_update_buddy_icon(PurpleBuddy *buddy);
static GList *skype_node_menu(PurpleBlistNode *node);
static void skype_silence(PurplePlugin *plugin, gpointer data);
void skype_slist_friend_check(gpointer buddy_pointer, gpointer friends_pointer);
int skype_slist_friend_search(gconstpointer buddy_pointer, gconstpointer buddyname_pointer);
static int skype_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags);
static void skype_chat_leave(PurpleConnection *gc, int id);
static void skype_chat_invite(PurpleConnection *gc, int id, const char *msg, const char *who);
static void skype_initiate_chat(PurpleBlistNode *node, gpointer data);
static void skype_set_chat_topic(PurpleConnection *gc, int id, const char *topic);
gchar *skype_cb_real_name(PurpleConnection *gc, int id, const char *who);
GList *skype_join_chat_info(PurpleConnection *gc);
void skype_alias_buddy(PurpleConnection *gc, const char *who, const char *alias);
gboolean skype_offline_msg(const PurpleBuddy *buddy);
//void skype_slist_remove_messages(gpointer buddy_pointer, gpointer unused);
static void skype_plugin_update_check(void);
void skype_plugin_update_callback(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);
gchar *timestamp_to_datetime(time_t timestamp);
void skype_show_search_users(PurplePluginAction *action);
static void skype_search_users(PurpleConnection *gc, const gchar *searchterm);
void skype_searchresults_add_buddy(PurpleConnection *gc, GList *row, void *user_data);
gchar *skype_strdup_withhtml(const gchar *src);
void skype_join_chat(PurpleConnection *, GHashTable *components);
gchar *skype_get_chat_name(GHashTable *components);
static void skype_display_skype_credit(PurplePluginAction *action);
unsigned int skype_send_typing(PurpleConnection *, const char *name, PurpleTypingState state);
static PurpleCmdRet skype_cmd_leave(PurpleConversation *, const gchar *, gchar **, gchar **, void *);
static PurpleCmdRet skype_cmd_topic(PurpleConversation *, const gchar *, gchar **, gchar **, void *);
static PurpleCmdRet skype_cmd_kick(PurpleConversation *, const gchar *, gchar **, gchar **, void *);
static PurpleCmdRet skype_cmd_kickban(PurpleConversation *, const gchar *, gchar **, gchar **, void *);
int skype_send_raw(PurpleConnection *, const char *, int);
void skype_group_buddy(PurpleConnection *, const char *who, const char *old_group, const char *new_group);
void skype_rename_group(PurpleConnection *, const char *old_name, PurpleGroup *group, GList *moved_buddies);
void skype_remove_group(PurpleConnection *, PurpleGroup *);
int skype_find_group_with_name(const char *group_name_in);
static gboolean skype_uri_handler(const char *proto, const char *cmd, GHashTable *params);
void skype_buddy_free(PurpleBuddy *buddy);
const char *skype_list_emblem(PurpleBuddy *buddy);
SkypeBuddy *skype_buddy_new(PurpleBuddy *buddy);
static void skype_open_sms_im(PurpleBlistNode *node, gpointer data);
void skype_got_buddy_icon_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);
gboolean skype_check_keepalive(PurpleConnection *gc);

#ifndef SKYPENET
static void skype_open_skype_options(void);
void skype_call_number(gpointer ignore, gchar *number);
void skype_call_number_request(PurplePlugin *plugin, gpointer data);
void skype_program_update_callback(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);
static void skype_program_update_check(void);
static void skype_send_file_from_blist(PurpleBlistNode *node, gpointer data);
static void skype_call_user_from_blist(PurpleBlistNode *node, gpointer data);
#endif


PurplePluginProtocolInfo prpl_info = {
	/* options */
#ifndef SKYPENET
#if _WIN32 || __APPLE__ || SKYPE_DBUS || !USE_XVFB_SERVER
	OPT_PROTO_NO_PASSWORD|
#endif
#endif
	OPT_PROTO_REGISTER_NOSCREENNAME|OPT_PROTO_CHAT_TOPIC|OPT_PROTO_SLASH_COMMANDS_NATIVE|OPT_PROTO_UNIQUE_CHATNAME,

	NULL,                /* user_splits */
	NULL,                /* protocol_options */
	{"png,gif,jpeg", 0, 0, 96, 96, 0, PURPLE_ICON_SCALE_SEND}, /* icon_spec */
	skype_list_icon,     /* list_icon */
	skype_list_emblem,   /* list_emblem */
	skype_status_text,   /* status_text */
	skype_tooltip_text,  /* tooltip_text */
	skype_status_types,  /* status_types */
	skype_node_menu,     /* blist_node_menu */
	skype_join_chat_info,/* chat_info */
	NULL,                /* chat_info_defaults */
	skype_login,         /* login */
	skype_close,         /* close */
	skype_send_im,       /* send_im */
	NULL,                /* set_info */
	skype_send_typing,   /* send_typing */
	skype_get_info,      /* get_info */
	skype_set_status,    /* set_status */
	skype_set_idle,      /* set_idle */
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
	skype_join_chat,     /* join_chat */
	NULL,                /* reject chat invite */
	skype_get_chat_name, /* get_chat_name */
	skype_chat_invite,   /* chat_invite */
	/*skype_chat_leave*/NULL,    /* chat_leave */
	NULL,                /* chat_whisper */
	skype_chat_send,     /* chat_send */
	skype_keepalive,     /* keepalive */
	NULL,                /* register_user */
	NULL,                /* get_cb_info */
	NULL,                /* get_cb_away */
	skype_alias_buddy,   /* alias_buddy */
	skype_group_buddy,   /* group_buddy */
	skype_rename_group,  /* rename_group */
	skype_buddy_free,    /* buddy_free */
	NULL,                /* convo_closed */
	skype_normalize,     /* normalize */
	skype_set_buddy_icon,/* set_buddy_icon */
	skype_remove_group,  /* remove_group */
	skype_cb_real_name,  /* get_cb_real_name */
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
	skype_send_raw,      /* send_raw */
	NULL,                /* roomlist_room_serialize */
	NULL,                /* unregister_user */
	NULL,                /* send_attention */
	NULL,                /* attention_types */
#ifndef __APPLE__
	(gpointer)
#endif
	sizeof(PurplePluginProtocolInfo) /* struct_size */
#ifdef USE_FARSIGHT
	, skype_media_initiate /* initiate_media */
#endif
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
/*	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
*/
	2, 1,
	PURPLE_PLUGIN_PROTOCOL, /* type */
	NULL, /* ui_requirement */
	0, /* flags */
	NULL, /* dependencies */
	PURPLE_PRIORITY_DEFAULT, /* priority */
#ifdef SKYPENET
	"prpl-bigbrownchunx-skypenet",
	"Skype (Network)",
#else
#ifdef SKYPE_DBUS
	"prpl-bigbrownchunx-skype-dbus", /* id */
	"Skype (D-Bus)",
#else
	"prpl-bigbrownchunx-skype", /* id */
	"Skype", /* name */
#endif
#endif
	"1.2", /* version */
	"Allows using Skype IM functions from within Pidgin", /* summary */
	"Allows using Skype IM functions from within Pidgin", /* description */
	"Eion Robb <eion@robbmob.com>", /* author */
	"http://eion.robbmob.com/", /* homepage */
	plugin_load, /* load */
	plugin_unload, /* unload */
	NULL, /* destroy */
	NULL, /* ui_info */
	&prpl_info, /* extra_info */
	NULL, /* prefs_info */
	skype_actions, /* actions */
	NULL, /* padding */
	NULL,
	NULL,
	NULL
};

static PurplePlugin *this_plugin;

static void
plugin_init(PurplePlugin *plugin)
{
	PurpleAccountOption *option;

#if _WIN32 && ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif

	if (!g_thread_supported ())
		g_thread_init (NULL);
	this_plugin = plugin;
	/* plugin's path at
		this_plugin->path */

#ifdef SKYPENET
	option = purple_account_option_string_new(_("Server"), "host", "skype.robbmob.com");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_int_new(_("Port"), "port", 5000);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#endif	

	option = purple_account_option_bool_new(_("Show SkypeOut contacts as 'Online'"), "skypeout_online", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_bool_new(_("Make Skype online/offline when going online/offline"), "skype_sync", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_bool_new(_("Automatically check for updates"), "check_for_updates", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#ifndef SKYPENET
	option = purple_account_option_bool_new(_("Auto-start Skype if not running"), "skype_autostart", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#endif	
	
	//leave
	purple_cmd_register("leave", "", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_IM |
						PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PRPL_ONLY |
						PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						plugin->info->id, skype_cmd_leave,
						_("leave [channel]:  Leave the chat"), NULL);
	//topic
	purple_cmd_register("topic", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						plugin->info->id, skype_cmd_topic,
						_("topic [&lt;new topic&gt;]:  View or change the topic"),
						NULL);
	//call, as in call person
	//kick
	purple_cmd_register("kick", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY,
						plugin->info->id, skype_cmd_kick,
						_("kick &lt;user&gt; [room]:  Kick a user from the room."),
						NULL);
	//kickban
	purple_cmd_register("kickban", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY,
						plugin->info->id, skype_cmd_kickban,
						_("kick &lt;user&gt; [room]:  Kick a user from the room."),
						NULL);
						
	purple_signal_connect(purple_get_core(), "uri-handler", plugin, PURPLE_CALLBACK(skype_uri_handler), NULL);
}

static PurpleCmdRet
skype_cmd_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *gc = NULL;
	int id = -1;
	
	gc = purple_conversation_get_gc(conv);	
	id = purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv));
	
	if (gc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	skype_chat_leave(gc, id);
	
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
skype_cmd_topic(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *gc = NULL;
	int id = -1;
	
	gc = purple_conversation_get_gc(conv);
	id = purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv));
	
	if (gc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	skype_set_chat_topic(gc, id, args ? args[0] : NULL);
	
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
skype_cmd_kick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	gchar *temp;
	gchar *chat_id;

	chat_id = (gchar *)g_hash_table_lookup(conv->data, "chat_id");
	temp = skype_send_message("ALTER CHAT %s KICK %s", chat_id, args[0]);
	if (!temp || !strlen(temp))
	{
		if(temp) g_free(temp);
		return PURPLE_CMD_RET_FAILED;
	}
	return PURPLE_CMD_RET_OK;	
}

static PurpleCmdRet
skype_cmd_kickban(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	gchar *temp;
	gchar *chat_id;

	chat_id = (gchar *)g_hash_table_lookup(conv->data, "chat_id");
	temp = skype_send_message("ALTER CHAT %s KICKBAN %s", chat_id, args[0]);
	if (!temp || !strlen(temp))
	{
		if(temp) g_free(temp);
		return PURPLE_CMD_RET_FAILED;
	}
	return PURPLE_CMD_RET_OK;	
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
	PurpleBuddy *buddy;
	SkypeBuddy *sbuddy;
	
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
#ifndef SKYPENET
#ifndef __APPLE__
		act = purple_menu_action_new(_("_Send File"),
										PURPLE_CALLBACK(skype_send_file_from_blist),
										NULL, NULL);
		m = g_list_append(m, act);
#endif		
		act = purple_menu_action_new(_("Call..."),
										PURPLE_CALLBACK(skype_call_user_from_blist),
										NULL, NULL);
		m = g_list_append(m, act);
#endif
		act = purple_menu_action_new(_("Initiate _Chat"),
							PURPLE_CALLBACK(skype_initiate_chat),
							NULL, NULL);
		m = g_list_append(m, act);

		buddy = (PurpleBuddy *)node;
		sbuddy = (SkypeBuddy *)buddy->proto_data;

		if (buddy->name[0] == '+' || (sbuddy && sbuddy->phone_mobile))
		{
			act = purple_menu_action_new(_("Send SMS"),
								PURPLE_CALLBACK(skype_open_sms_im),
								NULL, NULL);
			m = g_list_append(m, act);
		}
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node))
	{
		
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node))
	{
		act = purple_menu_action_new(_("Initiate _Chat"),
							PURPLE_CALLBACK(skype_initiate_chat),
							NULL, NULL);
		m = g_list_append(m, act);
	}
	return m;
}

#ifndef SKYPENET
static void
skype_send_file_from_blist(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;

	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		buddy = (PurpleBuddy *) node;
		if (PURPLE_BUDDY_IS_ONLINE(buddy))
		{
			skype_send_message_nowait("SET SILENT_MODE OFF");
			skype_send_message_nowait("OPEN FILETRANSFER %s", buddy->name);
			skype_send_message_nowait("SET SILENT_MODE ON");
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
		skype_send_message_nowait("CALL %s", buddy->name);
	}
}
#endif

const char *
skype_normalize(const PurpleAccount *acct, const char *who)
{
	static gchar *last_normalize = NULL;
	if (last_normalize)
		g_free(last_normalize);
	last_normalize = g_utf8_strdown(who, -1);
	return last_normalize;
}

GList *
skype_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	PurpleMenuAction *act;

#ifndef SKYPENET
	act = purple_menu_action_new(_("Hide Skype"),
									PURPLE_CALLBACK(skype_silence),
									NULL, NULL);
	m = g_list_append(m, act);
	act = purple_menu_action_new(_("Check for Skype updates..."),
									PURPLE_CALLBACK(skype_program_update_check),
									NULL, NULL);
	m = g_list_append(m, act);
#endif	
	if (this_plugin != NULL && this_plugin->path != NULL)
	{
		act = purple_menu_action_new(_("Check for plugin updates..."),
										PURPLE_CALLBACK(skype_plugin_update_check),
										NULL, NULL);
		m = g_list_append(m, act);
	}

	act = purple_menu_action_new(_("Search for buddies..."),
									PURPLE_CALLBACK(skype_show_search_users),
									NULL, NULL);
	m = g_list_append(m, act);

	act = purple_menu_action_new(_("Check Skype balance..."),
									PURPLE_CALLBACK(skype_display_skype_credit),
									NULL, NULL);
	m = g_list_append(m, act);
#ifndef SKYPENET	
	act = purple_menu_action_new(_("Call..."),
									PURPLE_CALLBACK(skype_call_number_request),
									NULL, NULL);
	m = g_list_append(m, act);
	
#ifndef __APPLE__
	act = purple_menu_action_new(_("Open Skype Options..."),
									PURPLE_CALLBACK(skype_open_skype_options),
									NULL, NULL);
	m = g_list_append(m, act);
#endif
#endif
	return m;
}

#ifndef SKYPENET
static void
skype_open_skype_options(void)
{
	skype_send_message_nowait("OPEN OPTIONS");
}
#endif

static void
skype_silence(PurplePlugin *plugin, gpointer data)
{
	skype_send_message_nowait("SET SILENT_MODE ON");
	skype_send_message_nowait("MINIMIZE");
	hide_skype();
}

static void
skype_plugin_update_check(void)
{
	gchar *basename;
	struct stat *filestat = g_new(struct stat, 1);

	//this_plugin is the PidginPlugin
	if (this_plugin == NULL || this_plugin->path == NULL || filestat == NULL || g_stat(this_plugin->path, filestat) == -1)
	{
		purple_notify_warning(this_plugin, "Warning", "Could not check for updates", NULL);
	} else {
		basename = g_path_get_basename(this_plugin->path);
		purple_util_fetch_url(g_strconcat("http://eion.robbmob.com/version?version=", basename, NULL),
			TRUE, NULL, FALSE, skype_plugin_update_callback, (gpointer)filestat);
	}
}

void
skype_plugin_update_callback(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	time_t mtime = ((struct stat *) user_data)->st_mtime;
	time_t servertime = atoi(url_text);
	skype_debug_info("skype", "Server filemtime: %d, Local filemtime: %d\n", servertime, mtime);
	if (servertime > mtime)
	{
		purple_notify_info(this_plugin, _("New Version Available"), _("There is a newer version of the Skype plugin available for download."), 
				g_strconcat(_("Your version"),": ",timestamp_to_datetime(mtime),"\n",_("Latest version"),": ",timestamp_to_datetime(servertime),"\nLatest version available from: ", this_plugin->info->homepage, NULL));
	} else {
		purple_notify_info(this_plugin, _("No updates found"), _("No updates found"), _("You have the latest version of the Skype plugin"));
	}
}

#ifndef SKYPENET
static void
skype_program_update_check(void)
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
		purple_notify_info(this_plugin, _("New Version Available"), _("There is a newer version of Skype available for download"), g_strconcat(_("Your version"),": ", version, "\n",_("Latest version"),": ", url_text, "\n\nhttp://www.skype.com/go/download", NULL));
	} else {
		purple_notify_info(this_plugin, _("No updates found"), _("No updates found"), _("You have the latest version of Skype"));
	}
}

void
skype_call_number_request(PurplePlugin *plugin, gpointer data)
{
	//http://developer.pidgin.im/doxygen/dev/html/request_8h.html#80ea2f9ad3a45471e05f09a2b5abcd75
	purple_request_input(plugin, _("Call..."), _("Enter the phone number or Skype buddy name to call"), NULL,
					NULL, FALSE, FALSE, NULL, _("Call..."), G_CALLBACK(skype_call_number), _("_Cancel"),
					NULL, NULL, NULL, NULL, NULL);
}

void
skype_call_number(gpointer ignore, gchar *number)
{
	skype_send_message_nowait("CALL %s", number);
}
#endif

GList *
skype_status_types(PurpleAccount *acct)
{
	GList *types;
	PurpleStatusType *status;

	skype_debug_info("skype", "returning status types\n");

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
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_AVAILABLE, "SkypeMe");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_AWAY, "Away");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_EXTENDED_AWAY, "Not Available");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_UNAVAILABLE, "Do Not Disturb");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_INVISIBLE, "Invisible");
	//_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_OFFLINE, "Offline");
	
	//Offline people shouldn't have status messages
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, _("Offline"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

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
	PurpleGroup *skype_group = NULL;
	PurpleGroup *skypeout_group = NULL;
	int i;

	friends_text = skype_send_message("SEARCH FRIENDS");
	if (strlen(friends_text) == 0)
	{
		g_free(friends_text);
		return FALSE;
	}
	//skip first word (should be USERS) and seperate by comma-space
	friends = g_strsplit((strchr(friends_text, ' ')+1), ", ", 0);
	g_free(friends_text);
	if (friends == NULL || friends[0] == NULL)
	{
		return FALSE;
	}
	
	//Remove from libpurple buddy list if not in skype friends list
	existing_friends = purple_find_buddies(acct, NULL);
	g_slist_foreach(existing_friends, (GFunc)skype_slist_friend_check, friends);
	
	//grab the list of buddy's again since they could have changed
	existing_friends = purple_find_buddies(acct, NULL);

	for (i=0; friends[i]; i++)
	{
		//If already in list, dont recreate, reuse
		skype_debug_info("skype", "Searching for friend %s\n", friends[i]);
		found_buddy = g_slist_find_custom(existing_friends,
											friends[i],
											skype_slist_friend_search);
		if (found_buddy != NULL)
		{
			//the buddy was already in the list
			buddy = (PurpleBuddy *)found_buddy->data;
			buddy->proto_data = skype_buddy_new(buddy);
			skype_debug_info("skype","Buddy already in list: %s (%s)\n", buddy->name, friends[i]);
		} else {
			skype_debug_info("skype","Buddy not in list %s\n", friends[i]);
			buddy = purple_buddy_new(acct, friends[i], NULL);
			buddy->proto_data = skype_buddy_new(buddy);
			
			//find out what group this buddy is in
			//for now, dump them into a default group, until skype tells us where they belong
			if (friends[i][0] == '+')
			{
				if (skypeout_group == NULL)
				{
					skypeout_group = purple_find_group("SkypeOut");
					if (skypeout_group == NULL)
					{
						skypeout_group = purple_group_new("SkypeOut");
						purple_blist_add_group(skypeout_group, NULL);
					}
				}
				purple_blist_add_buddy(buddy, NULL, skypeout_group, NULL);
			}
			else
			{
				if (skype_group == NULL)
				{
					skype_group = purple_find_group("Skype");
					if (skype_group == NULL)
					{
						skype_group = purple_group_new("Skype");
						purple_blist_add_group(skype_group, NULL);
					}
				}
				purple_blist_add_buddy(buddy, NULL, skype_group, NULL);
			}
		}
		skype_update_buddy_status(buddy);
		skype_update_buddy_alias(buddy);
		purple_prpl_got_user_idle(acct, buddy->name, FALSE, 0);

		//skype_update_buddy_icon(buddy);
	}
	
	//special case, if we're on our own buddy list
	if ((found_buddy = g_slist_find_custom(existing_friends, skype_get_account_username(acct), skype_slist_friend_search)))
	{
		buddy = (PurpleBuddy *)found_buddy->data;
		skype_update_buddy_status(buddy);
		skype_update_buddy_alias(buddy);
		purple_prpl_got_user_idle(acct, buddy->name, FALSE, 0);
		//skype_update_buddy_icon(buddy);
	}

	skype_debug_info("skype", "Friends Count: %d\n", i);
	g_strfreev(friends);
	
	skype_put_buddies_in_groups();
	
	return FALSE;
}

void
skype_slist_friend_check(gpointer buddy_pointer, gpointer friends_pointer)
{
	int i;
	PurpleBuddy *buddy = (PurpleBuddy *)buddy_pointer;
	char **friends = (char **)friends_pointer;

	if (g_str_equal(buddy->name, skype_get_account_username(buddy->account)))
	{
		//we must have put ourselves on our own list in pidgin, ignore
		return;
	}

	for(i=0; friends[i]; i++)
	{
		if (strlen(friends[i]) == 0)
			continue;
		if (g_str_equal(buddy->name, friends[i]))
			return;
	}
	skype_debug_info("skype", "removing buddy %d with name %s\n", buddy, buddy->name);
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

//create the memmem() function for platforms that dont support it
#if __APPLE__ || _WIN32
#ifndef _LIBC
# define __builtin_expect(expr, val)   (expr)
#endif

#undef memmem

/* Return the first occurrence of NEEDLE in HAYSTACK.  */
void *
memmem (haystack, haystack_len, needle, needle_len)
     const void *haystack;
     size_t haystack_len;
     const void *needle;
     size_t needle_len;
{
  const char *begin;
  const char *const last_possible
    = (const char *) haystack + haystack_len - needle_len;

  if (needle_len == 0)
    /* The first occurrence of the empty string is deemed to occur at
       the beginning of the string.  */
    return (void *) haystack;

  /* Sanity check, otherwise the loop might search through the whole
     memory.  */
  if (__builtin_expect (haystack_len < needle_len, 0))
    return NULL;

  for (begin = (const char *) haystack; begin <= last_possible; ++begin)
    if (begin[0] == ((const char *) needle)[0] &&
	!memcmp ((const void *) &begin[1],
		 (const void *) ((const char *) needle + 1),
		 needle_len - 1))
      return (void *) begin;

  return NULL;
}
#endif

void
skype_got_buddy_icon_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	PurpleBuddy *buddy = user_data;
	PurpleAccount *acct = buddy->account;
	
	if (error_message || !len || !url_text)
		return;
	
	purple_buddy_icons_set_for_user(acct, buddy->name, g_memdup(url_text,len), len, NULL);	
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
	/* -1 unknown, 0 none, 1 skype api, 2 dbb file, 3 url */
	static gint api_supports_avatar =
#ifndef SKYPENET
#	ifdef _WIN32
		1;
#	else
		2;
#	endif
#else
	3;
#endif
	
	if (api_supports_avatar == 0)
	{
		return;
	}
	
	skype_debug_info("skype", "Updating buddy icon for %s (%d)\n", buddy->name, api_supports_avatar);

	acct = purple_buddy_get_account(buddy);
	
	if (api_supports_avatar == 1 || api_supports_avatar == -1)
	{
		fh = g_file_open_tmp("skypeXXXXXX", &filename, &error);
		close(fh);
		
		if (filename != NULL)
		{
			new_filename = g_strconcat(filename, ".jpg", NULL);
			g_rename(filename, new_filename);
			ret = skype_send_message("GET USER %s AVATAR 1 %s", buddy->name, new_filename);
			if (strlen(ret) == 0)
			{
				skype_debug_warning("skype", "Error: Protocol doesn't suppot AVATAR\n");
			} else {
				if (g_file_get_contents(new_filename, &image_data, &image_data_len, NULL))
				{
					api_supports_avatar = 1;
					purple_buddy_icons_set_for_user(acct, buddy->name, image_data, image_data_len, NULL);
				}
			}
			g_free(ret);
			g_unlink(new_filename);
			g_free(filename);
			g_free(new_filename);
		} else {
			skype_debug_warning("skype", "Error making temp file %s\n", error->message);
			g_error_free(error);
		}
	}
	if (api_supports_avatar == 2 || api_supports_avatar == -1)
	{
		const gchar *userfiles[] = {"user256", "user1024", "user4096", "user16384", "user32768",
									"profile256", "profile1024", "profile4096", "profile16384", 
									NULL};
		char *username = g_strdup_printf("\x03\x10%s", buddy->name);
		for (fh = 0; userfiles[fh]; fh++)
		{
#ifdef _WIN32
			filename = g_strconcat(purple_home_dir(), "\\Skype\\", acct->username, "\\", userfiles[fh], ".dbb", NULL);
#else
#ifdef __APPLE__
			filename = g_strconcat(purple_home_dir(), "/Library/Application Support/Skype/", acct->username, "/", userfiles[fh], ".dbb", NULL);
#else
			filename = g_strconcat(purple_home_dir(), "/.Skype/", acct->username, "/", userfiles[fh], ".dbb", NULL);
#endif
#endif
			if (g_file_get_contents(filename, &image_data, &image_data_len, NULL))
			{
				api_supports_avatar = 2;
				char *start = (char *)memmem(image_data, image_data_len, username, strlen(username)+1);
				if (start != NULL)
				{
					char *next = image_data;
					char *last = next;
					//find last index of l33l
					while ((next = (char *)memmem(next+4, start-next-4, "l33l", 4)))
					{
						last = next;
					}
					start = last;
					if (start != NULL)
					{
						//find end of l33l block
						char *end = (char *)memmem(start+4, image_data+image_data_len-start-4, "l33l", 4);
						if (!end) end = image_data+image_data_len;
						
						//look for start of JPEG block
						char *img_start = (char *)memmem(start, end-start, "\xFF\xD8", 2);
						if (img_start)
						{
							//look for end of JPEG block
							char *img_end = (char *)memmem(img_start, end-img_start, "\xFF\xD9", 2);
							if (img_end)
							{
								image_data_len = img_end - img_start + 2;
								purple_buddy_icons_set_for_user(acct, buddy->name, g_memdup(img_start, image_data_len), image_data_len, NULL);
							}
						}
					}
				}
				g_free(image_data);
			}
			g_free(filename);
		}
		g_free(username);
	}
	if (api_supports_avatar == 3)
	{
		filename = g_strconcat("http://", purple_account_get_string(acct, "host", "skype.robbmob.com"), "/avatars/", buddy->name, NULL);
		purple_util_fetch_url(filename, TRUE, NULL, FALSE, skype_got_buddy_icon_cb, buddy);
		g_free(filename);
		return;
	}
	if (api_supports_avatar == -1)
	{
		api_supports_avatar = 0;
		return;
	}
}

const char *
skype_list_emblem(PurpleBuddy *buddy)
{
	SkypeBuddy *sbuddy;
	time_t now;
	struct tm *today_tm;
	
	if (buddy && buddy->proto_data)
	{
		sbuddy = buddy->proto_data;
		if (sbuddy->birthday)
		{
			now = time(NULL);
			today_tm = localtime(&now);
			if (sbuddy->birthday->tm_mday == today_tm->tm_mday &&
				sbuddy->birthday->tm_mon == today_tm->tm_mon &&
				sbuddy->birthday->tm_year == today_tm->tm_year)
			{
				return "birthday";
			}
		} 
	}
	return NULL;
}


void
skype_update_buddy_alias(PurpleBuddy *buddy)
{
	skype_send_message_nowait("GET USER %s DISPLAYNAME", buddy->name);
	skype_send_message_nowait("GET USER %s FULLNAME", buddy->name);
}

gboolean
skype_update_buddy_status(PurpleBuddy *buddy)
{
	PurpleAccount *acct;
	
	acct = purple_buddy_get_account(buddy);
	if (purple_account_is_connected(acct) == FALSE)
	{
		return FALSE;
	}
	skype_send_message_nowait("GET USER %s ONLINESTATUS", buddy->name);
	skype_send_message_nowait("GET USER %s MOOD_TEXT", buddy->name);
	skype_send_message_nowait("GET USER %s RICH_MOOD_TEXT", buddy->name);
	
	/* if this function was called from another thread, don't loop over it */
	return FALSE;
}

gboolean
skype_login_cb(gpointer acct)
{
	skype_login(acct);
	return FALSE;
}

void 
skype_login(PurpleAccount *acct)
{
	PurpleConnection *gc;
	gchar *reply;
	
	if (acct == NULL)
	{
		return;
	}
	
	if (purple_get_blist() == NULL)
	{
		return;
	}
	
	//set the account, with misleading function name :)
	skype_get_account(acct);

	gc = purple_account_get_connection(acct);
	
	if (gc == NULL)
		return;
	
	gc->flags = PURPLE_CONNECTION_NO_BGCOLOR |
				PURPLE_CONNECTION_NO_URLDESC |
				PURPLE_CONNECTION_NO_FONTSIZE |
				PURPLE_CONNECTION_NO_IMAGES;

	/* 1. connect to server */
	/* TODO: Work out if a skype connection is already running */
	//purple_account_is_connected(acct)
	if (FALSE)
	{
		purple_connection_error(gc, g_strconcat("\n",_("Only one Skype account allowed"), NULL));
		return;
	}
	
	purple_connection_update_progress(gc, _("Connecting"), 0, 5);
	
	if (!skype_connect())
	{
		if (purple_account_get_bool(acct, "skype_autostart", TRUE))
		{
			skype_debug_info("skype", "Should I start Skype?\n");
			if (!is_skype_running())
			{
				skype_debug_info("skype", "Yes, start Skype\n");
				exec_skype();
				purple_timeout_add_seconds(10, skype_login_cb, acct);
				return;
				gc->wants_to_die = FALSE;
			}
		} else {
			gc->wants_to_die = TRUE;
		}
		purple_connection_error(gc, g_strconcat("\n", _("Could not connect to Skype process.\nSkype not running?"), NULL));		
		return;
	}
	
	purple_connection_update_progress(gc, _("Authorizing"),
								  1,   /* which connection step this is */
								  5);  /* total number of steps */

	

#if SKYPENET || ! __APPLE__
#ifdef __APPLE__
	reply = skype_send_message("NAME Adium");
#else
	reply = skype_send_message("NAME %s", g_get_application_name());
#endif
	if (reply == NULL || strlen(reply) == 0)
	{
		purple_connection_error(gc, g_strconcat("\n",_("Skype client not ready"), NULL));
		return;
	}
	if (g_str_equal(reply, "CONNSTATUS OFFLINE"))
	{
		//this happens if we connect before skype has connected to the network
		purple_timeout_add_seconds(1, skype_login_cb, acct);
		g_free(reply);
		return;
	}
	g_free(reply);
#endif
	purple_connection_update_progress(gc, _("Initializing"), 2, 5);
	reply = skype_send_message("PROTOCOL 7");
	if (reply == NULL || strlen(reply) == 0)
	{
		purple_connection_error(gc, g_strconcat("\n",_("Skype client not ready"), NULL));
		return;
	}
	g_free(reply);
	
	purple_connection_update_progress(gc, _("Hide Skype"), 3, 5);
	skype_silence(NULL, NULL);
	purple_connection_update_progress(gc, _("Connected"), 4, 5);

	skype_get_account_alias(acct);
	skype_get_account_username(acct);
	if (purple_account_get_bool(acct, "skype_sync", TRUE))
		skype_set_status(acct, purple_account_get_active_status(acct));
	//sync buddies after everything else has finished loading
	purple_timeout_add(10, (GSourceFunc)skype_set_buddies, (gpointer)acct);
	skype_send_message_nowait("CREATE APPLICATION libpurple_typing");
	
	purple_connection_set_state(gc, PURPLE_CONNECTED);
}

const char *
skype_get_account_username(PurpleAccount *acct)
{
#ifdef SKYPENET
	if (acct == NULL)
		return NULL;
	return acct->username;
#else
	char *ret;
	static char *username = NULL;
	
	if (username != NULL)
		return username;
	
	if (!acct)
		return "Skype";
	
	ret = skype_send_message("GET CURRENTUSERHANDLE");
	if (!ret || !strlen(ret))
	{
		g_free(ret);
		return NULL;
	}
	username = g_strdup(&ret[18]);
	g_free(ret);

	if (acct && !g_str_equal(acct->username, username))
	{
		skype_debug_info("skype", "Setting username to %s\n", username);
		purple_account_set_username(acct, username);
	}
	return username;
#endif
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
	g_free(alias);
}

void 
skype_close(PurpleConnection *gc)
{
	//GSList *buddies;

	skype_debug_info("skype", "logging out\n");
	if (gc && purple_account_get_bool(gc->account, "skype_sync", TRUE))
		skype_send_message_nowait("SET USERSTATUS OFFLINE");
	skype_send_message_nowait("SET SILENT_MODE OFF");
	skype_disconnect();
	/*if (gc)
	{
		buddies = purple_find_buddies(gc->account, NULL);
		if (buddies != NULL && g_slist_length(buddies) > 0)
			g_slist_foreach(buddies, skype_slist_remove_messages, NULL);
	}*/
}

int 
skype_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, 
		PurpleMessageFlags flags)
{
	char *stripped;
	char *chat_id;
	PurpleConversation *conv;
	
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, purple_connection_get_account(gc));
	
	if (who[0] == '+' && conv && purple_conversation_get_data(conv, "sms_msg"))
	{
		return skype_send_sms(gc, who, message, flags);
	}

	stripped = purple_markup_strip_html(message);
	
	if (conv == NULL || purple_conversation_get_data(conv, "chat_id") == NULL)
	{
		/*chat_id = g_new(char, 200);
		char *temp;
		temp = skype_send_message("CHAT CREATE %s", who);
		sscanf(temp, "CHAT %s ", chat_id);
		g_free(temp);
		if (conv != NULL)
		{
			purple_conversation_set_data(conv, "chat_id", chat_id);
		}*/
		skype_send_message_nowait("MESSAGE %s %s", who, stripped);
	} else {
		chat_id = purple_conversation_get_data(conv, "chat_id");
		skype_send_message_nowait("CHATMESSAGE %s %s", chat_id, stripped);
	}


#ifdef USE_SKYPE_SENT
	return 0;
#else
	return 1;
#endif
}


unsigned int
skype_send_typing(PurpleConnection *gc, const gchar *name, PurpleTypingState state)
{
	gchar *string_state = NULL, *temp = NULL;
	PurpleAccount *account;
	account = purple_connection_get_account(gc);
	
	//Don't send typing messages to SkypeOut
	if (name[0] == '+')
		return 0;
	
	switch(state)
	{
		case PURPLE_NOT_TYPING:
			string_state = "PURPLE_NOT_TYPING";
			break;
		case PURPLE_TYPING:
			string_state = "PURPLE_TYPING";
			break;
		case PURPLE_TYPED:
			string_state = "PURPLE_TYPED";
			break;
		default:
			string_state = "";
			break;
	}
	
	temp = g_strconcat("stream-", name, NULL);
	
	//lets be dumb and try to connect without getting a stream
	skype_send_message_nowait("CREATE APPLICATION libpurple_typing");
	skype_send_message_nowait( "ALTER APPLICATION libpurple_typing CONNECT %s", name);
	skype_send_message_nowait( "ALTER APPLICATION libpurple_typing DATAGRAM %s:%s %s", name, 
					purple_account_get_string(account, temp, "1"), string_state);
	
	g_free(temp);
	
	return 4;
}

gchar *
timestamp_to_datetime(time_t timestamp)
{
	return g_strdup(purple_date_format_long(localtime(&timestamp)));
}

void
set_skype_buddy_attribute(SkypeBuddy *sbuddy, const gchar *skype_buddy_property, const gchar *value)
{
	if (sbuddy == NULL)
		return;
	if (skype_buddy_property == NULL)
		return;

	if (g_str_equal(skype_buddy_property, "FULLNAME"))
	{
		if (sbuddy->fullname)
			g_free(sbuddy->fullname);
		sbuddy->fullname = NULL;
		if (value && strlen(value))
			sbuddy->fullname = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "MOOD_TEXT"))
	{
		if (sbuddy->mood)
			g_free(sbuddy->mood);
		sbuddy->mood = NULL;
		if (value)
		{
			sbuddy->mood = g_strdup(value);
			purple_util_chrreplace(sbuddy->mood, '\n', ' ');
		}
	} else if (g_str_equal(skype_buddy_property, "BIRTHDAY"))
	{
		if (sbuddy->birthday)
			g_free(sbuddy->birthday);
		sbuddy->birthday = NULL;
		if (value && strlen(value) && !g_str_equal(value, "0"))
		{
			sbuddy->birthday = g_new(struct tm, 1);
			purple_str_to_time(value, FALSE, sbuddy->birthday, NULL, NULL);
		}
	} else if (g_str_equal(skype_buddy_property, "SEX"))
	{
		if (sbuddy->gender)
			g_free(sbuddy->gender);
		sbuddy->gender = NULL;
		if (value && strlen(value) && !g_str_equal(value, "UNKNOWN"))
			sbuddy->gender = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "LANGUAGE"))
	{
		if (sbuddy->language)
			g_free(sbuddy->language);
		sbuddy->language = NULL;
		if (value && strlen(value))
			sbuddy->language = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "COUNTRY"))
	{
		if (sbuddy->country)
			g_free(sbuddy->country);
		sbuddy->country = NULL;
		if (value && strlen(value))
			sbuddy->country = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "IS_VIDEO_CAPABLE"))
	{
		if (value && strlen(value) && g_ascii_strcasecmp(value, "TRUE"))
			sbuddy->is_video_capable = TRUE;
		else
			sbuddy->is_video_capable = FALSE;
	} else if (g_str_equal(skype_buddy_property, "ISAUTHORIZED"))
	{
		if (value && strlen(value) && g_ascii_strcasecmp(value, "TRUE"))
			sbuddy->is_authorized = TRUE;
		else
			sbuddy->is_authorized = FALSE;
	} else if (g_str_equal(skype_buddy_property, "ISBLOCKED"))
	{
		if (value && strlen(value) && g_ascii_strcasecmp(value, "TRUE"))
			sbuddy->is_blocked = TRUE;
		else
			sbuddy->is_blocked = FALSE;
	} else if (g_str_equal(skype_buddy_property, "LASTONLINETIMESTAMP"))
	{
		sbuddy->last_online = 0;
		if (value)
			sbuddy->last_online = (time_t) atoi(value);
	} else if (g_str_equal(skype_buddy_property, "TIMEZONE"))
	{
		sbuddy->timezone_offset = 0;
		if (value)
			sbuddy->timezone_offset = (g_ascii_strtod(value, NULL) / 3600) - 24;
	} else if (g_str_equal(skype_buddy_property, "NROF_AUTHED_BUDDIES"))
	{
		sbuddy->number_of_buddies = 0;
		if (value)
			sbuddy->number_of_buddies = g_ascii_strtoull(value, NULL, 10);
	} else if (g_str_equal(skype_buddy_property, "ABOUT"))
	{
		if (sbuddy->about)
			g_free(sbuddy->about);
		sbuddy->about = NULL;
		if (value && strlen(value))
		{
			sbuddy->about = g_strdup(value);
		}
	} else if (g_str_equal(skype_buddy_property, "PROVINCE"))
	{
		if (sbuddy->province)
			g_free(sbuddy->province);
		sbuddy->province = NULL;
		if (value && strlen(value))
		{
			sbuddy->province = g_strdup(value);
		}
	} else if (g_str_equal(skype_buddy_property, "CITY"))
	{
		if (sbuddy->city)
			g_free(sbuddy->city);
		sbuddy->city = NULL;
		if (value && strlen(value))
			sbuddy->city = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "PHONE_HOME"))
	{
		if (sbuddy->phone_home)
			g_free(sbuddy->phone_home);
		sbuddy->phone_home = NULL;
		if (value && strlen(value))
			sbuddy->phone_home = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "PHONE_OFFICE"))
	{
		if (sbuddy->phone_office)
			g_free(sbuddy->phone_office);
		sbuddy->phone_office = NULL;
		if (value && strlen(value))
			sbuddy->phone_office = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "PHONE_MOBILE"))
	{
		if (sbuddy->phone_mobile)
			g_free(sbuddy->phone_mobile);
		sbuddy->phone_mobile = NULL;
		if (value && strlen(value))
			sbuddy->phone_mobile = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "HOMEPAGE"))
	{
		if (sbuddy->homepage)
			g_free(sbuddy->homepage);
		sbuddy->homepage = NULL;
		if (value && strlen(value))
			sbuddy->homepage = g_strdup(value);
	} else if (g_str_equal(skype_buddy_property, "HASCALLEQUIPMENT"))
	{
		if (value && strlen(value) && g_ascii_strcasecmp(value, "TRUE"))
			sbuddy->has_call_equipment = TRUE;
		else
			sbuddy->has_call_equipment = FALSE;
	} else if (g_str_equal(skype_buddy_property, "IS_VIDEO_CAPABLE"))
	{
		if (value && strlen(value) && g_ascii_strcasecmp(value, "TRUE"))
			sbuddy->is_video_capable = TRUE;
		else
			sbuddy->is_video_capable = FALSE;
	} else if (g_str_equal(skype_buddy_property, "IS_VOICEMAIL_CAPABLE"))
	{
		if (value && strlen(value) && g_ascii_strcasecmp(value, "TRUE"))
			sbuddy->is_voicemail_capable = TRUE;
		else
			sbuddy->is_voicemail_capable = FALSE;
	} else if (g_str_equal(skype_buddy_property, "CAN_LEAVE_VM"))
	{
		if (value && strlen(value) && g_ascii_strcasecmp(value, "TRUE"))
			sbuddy->can_leave_voicemail = TRUE;
		else
			sbuddy->can_leave_voicemail = FALSE;
	}
}

SkypeBuddy *
skype_buddy_new(PurpleBuddy *buddy)
{
	SkypeBuddy *newbuddy = g_new(SkypeBuddy, 1);
	newbuddy->buddy = buddy;
	
	//this bit used in Get Info
	newbuddy->handle = g_strdup(buddy->name);
	newbuddy->fullname = NULL;
	newbuddy->mood = NULL;
	newbuddy->birthday = NULL;
	newbuddy->gender = NULL;
	newbuddy->language = NULL;
	newbuddy->country = NULL;
	newbuddy->is_video_capable = FALSE;
	newbuddy->is_authorized = FALSE;
	newbuddy->is_blocked = FALSE;
	newbuddy->last_online = 0;
	newbuddy->timezone_offset = 0;
	newbuddy->number_of_buddies = 0;
	newbuddy->about = NULL;
	
	//this bit not used yet
	newbuddy->province = NULL;
	newbuddy->city = NULL;
	newbuddy->phone_home = NULL;
	newbuddy->phone_office = NULL;
	newbuddy->phone_mobile = NULL;
	newbuddy->homepage = NULL;
	newbuddy->has_call_equipment = FALSE;
	newbuddy->is_voicemail_capable = FALSE;
	newbuddy->is_callforward_active = FALSE;
	newbuddy->can_leave_voicemail = FALSE;
	
	//libpurple buddy stuff
	newbuddy->typing_stream = 0;
	
	skype_send_message_nowait("GET USER %s FULLNAME", buddy->name);
	skype_send_message_nowait("GET USER %s MOOD_TEXT", buddy->name);
	skype_send_message_nowait("GET USER %s BIRTHDAY", buddy->name);
	skype_send_message_nowait("GET USER %s SEX", buddy->name);
	skype_send_message_nowait("GET USER %s LANGUAGE", buddy->name);
	skype_send_message_nowait("GET USER %s COUNTRY", buddy->name);
	skype_send_message_nowait("GET USER %s IS_VIDEO_CAPABLE", buddy->name);
	skype_send_message_nowait("GET USER %s ISAUTHORIZED", buddy->name);
	skype_send_message_nowait("GET USER %s ISBLOCKED", buddy->name);
	skype_send_message_nowait("GET USER %s LASTONLINETIMESTAMP", buddy->name);
	skype_send_message_nowait("GET USER %s TIMEZONE", buddy->name);
	skype_send_message_nowait("GET USER %s NROF_AUTHED_BUDDIES", buddy->name);
	skype_send_message_nowait("GET USER %s ABOUT", buddy->name);
	
	skype_send_message_nowait("GET USER %s PROVINCE", buddy->name);
	skype_send_message_nowait("GET USER %s CITY", buddy->name);
	skype_send_message_nowait("GET USER %s PHONE_HOME", buddy->name);
	skype_send_message_nowait("GET USER %s PHONE_OFFICE", buddy->name);
	skype_send_message_nowait("GET USER %s PHONE_MOBILE", buddy->name);
	skype_send_message_nowait("GET USER %s HOMEPAGE", buddy->name);
	skype_send_message_nowait("GET USER %s HASCALLEQUIPMENT", buddy->name);
	skype_send_message_nowait("GET USER %s IS_VIDEO_CAPABLE", buddy->name);
	skype_send_message_nowait("GET USER %s IS_VOICEMAIL_CAPABLE", buddy->name);
	skype_send_message_nowait("GET USER %s CAN_LEAVE_VM", buddy->name);
	
	buddy->proto_data = newbuddy;
	return newbuddy;
}

void
skype_buddy_free(PurpleBuddy *buddy)
{
	SkypeBuddy *sbuddy;
	
	if (buddy->proto_data)
	{
		sbuddy = buddy->proto_data;
		buddy->proto_data = NULL;
		
		if (sbuddy->handle) g_free(sbuddy->handle);
		if (sbuddy->fullname) g_free(sbuddy->fullname);
		if (sbuddy->mood) g_free(sbuddy->mood);
		if (sbuddy->birthday) g_free(sbuddy->birthday);
		if (sbuddy->gender) g_free(sbuddy->gender);
		if (sbuddy->language) g_free(sbuddy->language);
		if (sbuddy->country) g_free(sbuddy->country);
		if (sbuddy->about) g_free(sbuddy->about);
		if (sbuddy->province) g_free(sbuddy->province);
		if (sbuddy->city) g_free(sbuddy->city);
		if (sbuddy->phone_home) g_free(sbuddy->phone_home);
		if (sbuddy->phone_office) g_free(sbuddy->phone_office);
		if (sbuddy->phone_mobile) g_free(sbuddy->phone_mobile);
		if (sbuddy->homepage) g_free(sbuddy->homepage);
		
		g_free(sbuddy);
	}
}

void 
skype_get_info(PurpleConnection *gc, const gchar *username)
{
	PurpleNotifyUserInfo *user_info;
	double timezoneoffset = 0;
	struct tm *birthday_time;
	int time;
	gchar *temp;
	PurpleBuddy *buddy;
	SkypeBuddy *sbuddy;
	
	buddy = purple_find_buddy(gc->account, username);
	if (buddy && buddy->proto_data)
	{
		sbuddy = buddy->proto_data;
		user_info = purple_notify_user_info_new();
		
		purple_notify_user_info_add_section_header(user_info, _("Contact Info"));
		purple_notify_user_info_add_pair(user_info, _("Skype Name"), buddy->name);
		purple_notify_user_info_add_pair(user_info, _("Full Name"), sbuddy->fullname);
		purple_notify_user_info_add_pair(user_info, _("Mood Text"), sbuddy->mood);
		
		purple_notify_user_info_add_section_break(user_info);
		
		purple_notify_user_info_add_section_header(user_info, _("Personal Information"));
		purple_notify_user_info_add_pair(user_info, _("Birthday"), purple_date_format_short(sbuddy->birthday));
		purple_notify_user_info_add_pair(user_info, _("Gender"), sbuddy->gender);
		purple_notify_user_info_add_pair(user_info, _("Preferred Language"), sbuddy->language);
		purple_notify_user_info_add_pair(user_info, _("Country"), sbuddy->country);
		purple_notify_user_info_add_pair(user_info, _("Is Video Capable"), (sbuddy->is_video_capable?"TRUE":"FALSE"));
		purple_notify_user_info_add_pair(user_info, _("Authorization Granted"), (sbuddy->is_authorized?"TRUE":"FALSE"));
		purple_notify_user_info_add_pair(user_info, _("Blocked"), (sbuddy->is_blocked?"TRUE":"FALSE"));
		if (sbuddy->timezone_offset)
		{
			purple_notify_user_info_add_pair(user_info, _("Timezone"), temp = g_strdup_printf("UMT %+.1f", sbuddy->timezone_offset));
			g_free(temp);
		} else {
			purple_notify_user_info_add_pair(user_info, _("Timezone"), NULL);
		}
		purple_notify_user_info_add_pair(user_info, _("Number of buddies"), temp = g_strdup_printf("%d", sbuddy->number_of_buddies));
		g_free(temp);
		
		purple_notify_user_info_add_section_break(user_info);
		
		purple_notify_user_info_add_pair(user_info, NULL, sbuddy->about);
	
		purple_notify_userinfo(gc, username, user_info, NULL, NULL);
		purple_notify_user_info_destroy(user_info);
		
		return;
	}
	
	user_info = purple_notify_user_info_new();
#define _SKYPE_USER_INFO(prop, key)  \
	purple_notify_user_info_add_pair(user_info, _(key),\
		(skype_get_user_info(username, prop)));

	purple_notify_user_info_add_section_header(user_info, _("Contact Info"));
	_SKYPE_USER_INFO("HANDLE", "Skype Name");
	_SKYPE_USER_INFO("FULLNAME", "Full Name");
	purple_notify_user_info_add_section_break(user_info);
	purple_notify_user_info_add_section_header(user_info, _("Personal Information"));
	//_SKYPE_USER_INFO("BIRTHDAY", "Birthday");
	temp = skype_get_user_info(username, "BIRTHDAY");
	if (temp && strlen(temp) && !g_str_equal(temp, "0"))
	{
		birthday_time = g_new(struct tm, 1);
		purple_str_to_time(temp, FALSE, birthday_time, NULL, NULL);
		purple_notify_user_info_add_pair(user_info, _("Birthday"), g_strdup(purple_date_format_short(birthday_time)));
		g_free(birthday_time);
	} else {
		purple_notify_user_info_add_pair(user_info, _("Birthday"), g_strdup("0"));
	}
	_SKYPE_USER_INFO("SEX", "Gender");
	_SKYPE_USER_INFO("LANGUAGE", "Preferred Language");
	_SKYPE_USER_INFO("COUNTRY", "Country");
	_SKYPE_USER_INFO("IS_VIDEO_CAPABLE", "Is Video Capable");
	_SKYPE_USER_INFO("ISAUTHORIZED", "Authorization Granted");
	_SKYPE_USER_INFO("ISBLOCKED", "Blocked");
	//_SKYPE_USER_INFO("LASTONLINETIMESTAMP", "Last online"); //timestamp
	time = atoi(skype_get_user_info(username, "LASTONLINETIMESTAMP"));
	skype_debug_info("skype", "time: %d\n", time);
	purple_notify_user_info_add_pair(user_info, _("Last online"),
			timestamp_to_datetime((time_t) time));
	//		g_strdup(purple_date_format_long(localtime((time_t *)(void *)&time))));
	//_SKYPE_USER_INFO("TIMEZONE", "Timezone"); //in seconds
	timezoneoffset = atof(skype_get_user_info(username, "TIMEZONE")) / 3600;
	timezoneoffset -= 24; //timezones are offset by 24 hours to keep them valid and unsigned
	purple_notify_user_info_add_pair(user_info, _("Timezone"), g_strdup_printf("UMT %+.1f", timezoneoffset));
	
	_SKYPE_USER_INFO("NROF_AUTHED_BUDDIES", "Number of buddies");
	purple_notify_user_info_add_section_break(user_info);
	//_SKYPE_USER_INFO("ABOUT", "");
	purple_notify_user_info_add_pair(user_info, NULL, (skype_get_user_info(username, "ABOUT")));
	
	purple_notify_userinfo(gc, username, user_info, NULL, NULL);
	purple_notify_user_info_destroy(user_info);
	
}

gchar *
skype_get_user_info(const gchar *username, const gchar *property)
{
	gchar *outstr;
	gchar *return_str;
	outstr = skype_send_message("GET USER %s %s", username, property);
	if (strlen(outstr) == 0)
		return outstr;
	return_str = g_strdup(&outstr[7+strlen(username)+strlen(property)]);
	g_free(outstr);
	/* skype_debug_info("skype", "User %s's %s is %s", username, property, return_str); */
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
	else
		message = purple_markup_strip_html(message);
	skype_send_message_nowait("SET PROFILE MOOD_TEXT %s", message);
}

void
skype_set_idle(PurpleConnection *gc, int time)
{
	skype_send_message_nowait("SET AUTOAWAY OFF");
	skype_send_message_nowait("RESETIDLETIMER");
}


void
skype_put_buddies_in_groups()
{
	skype_send_message_nowait("SEARCH GROUPS CUSTOM");
}

gboolean
groups_table_find_group(gpointer key, gpointer value, gpointer user_data)
{
	gchar *group_name = user_data;
	gint group_number = GPOINTER_TO_INT(key);
	PurpleGroup *group = value;
	if (g_str_equal(group_name, group->name))
	{
		purple_blist_node_set_int(&group->node, "skype_group_number", group_number);
		return TRUE;
	}
	return FALSE;
}

int
skype_find_group_with_name(const char *group_name_in)
{
	PurpleGroup *purple_group;
	gint group_number;

	purple_group = purple_find_group(group_name_in);
	if (purple_group != NULL)
	{
		group_number = purple_blist_node_get_int(&purple_group->node, "skype_group_number");
		if (group_number)
			return group_number;
	}
	
	if (groups_table == NULL)
	{
		skype_send_message_nowait("SEARCH GROUPS CUSTOM");
		return 0;
	}
	
	purple_group = g_hash_table_find(groups_table, groups_table_find_group, (gpointer)group_name_in);
	if (purple_group == NULL)
	{
		return 0;
	}
	return purple_blist_node_get_int(&purple_group->node, "skype_group_number");
}

//use this to try and regroup a buddy after a few seconds
struct _cheat_skype_group_buddy_struct {
	PurpleConnection *gc;
	const char *who;
	const char *old_group;
	const char *new_group;
};

gboolean
skype_group_buddy_timeout(struct _cheat_skype_group_buddy_struct *cheat)
{
	if (!cheat)
		return FALSE;
	
	skype_group_buddy(cheat->gc, cheat->who, cheat->old_group, cheat->new_group);
	g_free(cheat);
	return FALSE;
}

void
skype_group_buddy(PurpleConnection *gc, const char *who, const char *old_group, const char *new_group)
{
	int group_number = 0;

	//add to new group
	group_number = skype_find_group_with_name(new_group);
	if (!group_number)
	{
		skype_send_message_nowait("CREATE GROUP %s", new_group);
		struct _cheat_skype_group_buddy_struct *cheat = g_new(struct _cheat_skype_group_buddy_struct, 1);
		cheat->gc = gc;
		cheat->who = who;
		cheat->old_group = old_group;
		cheat->new_group = new_group;
		purple_timeout_add_seconds(5, (GSourceFunc)skype_group_buddy_timeout, cheat);
		return;
	}
	
	skype_send_message_nowait("ALTER GROUP %d ADDUSER %s", group_number, who);
	
	if (old_group == NULL)
		return;
	
	//remove from old group
	group_number = skype_find_group_with_name(old_group);
	if (!group_number)
		return;
	skype_send_message_nowait("ALTER GROUP %d REMOVEUSER %s", group_number, who);
}

void
skype_rename_group(PurpleConnection *gc, const char *old_name, PurpleGroup *group, GList *moved_buddies)
{
	int group_number = 0;
	
	group_number = skype_find_group_with_name(old_name);
	if (!group_number)
	{
		group->name = g_strdup(old_name);
		return;
	}
	skype_send_message_nowait("SET GROUP %d DISPLAYNAME %s", group_number, group->name);
}

void skype_remove_group(PurpleConnection *gc, PurpleGroup *group)
{
	int group_number = 0;
	
	group_number = skype_find_group_with_name(group->name);
	skype_send_message_nowait("DELETE GROUP %d", group_number);
}

void 
skype_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	skype_send_message_nowait("SET USER %s BUDDYSTATUS 2 %s", buddy->name, _("Please authorize me so I can add you to my buddy list."));
	if (buddy->alias == NULL || strlen(buddy->alias) == 0)
		skype_update_buddy_alias(buddy);
	else
		skype_alias_buddy(gc, buddy->name, buddy->alias);
	if (group && group->name)
	{
		skype_group_buddy(gc, buddy->name, NULL, group->name);
	}
	skype_add_permit(gc, buddy->name);
	skype_rem_deny(gc, buddy->name);
	
	skype_update_buddy_status(buddy);
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

static time_t last_ping = 0;

gboolean
skype_check_keepalive(PurpleConnection *gc)
{
	if (last_pong < last_ping)
		purple_connection_error(gc, _("\nSkype not responding"));
	return FALSE;
}

void
skype_keepalive(PurpleConnection *gc)
{
	last_ping = time(NULL);
	skype_send_message_nowait("PING");
	purple_timeout_add_seconds(10, (GSourceFunc)skype_check_keepalive, gc);
}

void
skype_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img)
{
	gchar *path;
	if (img != NULL)
	{
		path = g_build_filename(purple_buddy_icons_get_cache_dir(), purple_imgstore_get_filename(img), NULL);
		skype_send_message_nowait("SET AVATAR 1 %s:1", path);
	}
	else
		skype_send_message_nowait("SET AVATAR 1");
}

char *
skype_status_text(PurpleBuddy *buddy)
{
	char *mood_text;
	PurplePresence *presence;
	PurpleStatus *status;
	PurpleStatusType *type;
	SkypeBuddy *sbuddy = buddy->proto_data;
	
	if (sbuddy && sbuddy->mood != NULL && strlen(sbuddy->mood))
		return skype_strdup_withhtml(sbuddy->mood);
		
	if (sbuddy == NULL || sbuddy->mood == NULL)
	{
		skype_send_message_nowait("GET USER %s MOOD_TEXT", buddy->name);
		return NULL;
	}

	//If we're at this point, they don't have a mood.
	//Use away status instead
	presence = purple_buddy_get_presence(buddy);
	if (presence == NULL)
		return NULL;
	status = purple_presence_get_active_status(presence);
	if (status == NULL)
		return NULL;
	type = purple_status_get_type(status);
	if (type == NULL || 
		purple_status_type_get_primitive(type) == PURPLE_STATUS_AVAILABLE ||
		purple_status_type_get_primitive(type) == PURPLE_STATUS_OFFLINE)
			return NULL;
	mood_text = (char *)purple_status_type_get_name(type);
	if (mood_text != NULL && strlen(mood_text))
		return skype_strdup_withhtml(mood_text);

	return NULL;
}

void
skype_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *userinfo, gboolean full)
{
	PurplePresence *presence;
	PurpleStatus *status;
	SkypeBuddy *sbuddy = buddy->proto_data;

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);
	purple_notify_user_info_add_pair(userinfo, _("Status"), purple_status_get_name(status));
	if (sbuddy && sbuddy->mood && strlen(sbuddy->mood))
		purple_notify_user_info_add_pair(userinfo, _("Message"), sbuddy->mood);
}

const char *
skype_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	//when pidgin requests the icon for a skypeout buddy, it also requests the icon
	//for the account, which overwrites the buddy's skypeout icon, so cheat
	//and store it :)
	static gboolean last_icon_was_skypeout = FALSE;

	if (last_icon_was_skypeout)
	{
		last_icon_was_skypeout = FALSE;
		return "skypeout";
	}

	if (buddy && buddy->name[0] == '+')
	{
		last_icon_was_skypeout = TRUE;
		return "skypeout";
	}
	return "skype";
}

int
skype_send_raw(PurpleConnection *gc, const char *buf, int len)
{
	gchar *newbuf = NULL;
	int newlen = 0;
	
	newbuf = g_strndup(buf, len);
	newlen = strlen(newbuf);
	if (newbuf != NULL)
	{
		skype_send_message_nowait(newbuf);
		g_free(newbuf);
	}
	
	return newlen;
}

static void
skype_initiate_chat(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	//PurpleGroup *group;
	PurpleConversation *conv;
	gchar *msg;
	gchar chat_id[200];
	static int chat_number = 1000;

	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		buddy = (PurpleBuddy *) node;
		msg = skype_send_message("CHAT CREATE");
		sscanf(msg, "CHAT %s ", chat_id);
		g_free(msg);
		
		conv = serv_got_joined_chat(purple_account_get_connection(purple_buddy_get_account(buddy)), chat_number, chat_id);
		skype_send_message_nowait("ALTER CHAT %s ADDMEMBERS %s", chat_id, buddy->name);
		skype_debug_info("skype", "Conv Hash Table: %d\n", conv->data);
		skype_debug_info("skype", "chat_id: %s\n", chat_id);
		purple_conversation_set_data(conv, "chat_id", g_strdup(chat_id));
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),
									skype_get_account_username(buddy->account), NULL, PURPLE_CBFLAGS_NONE, FALSE);
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),
									buddy->name, NULL, PURPLE_CBFLAGS_NONE, FALSE);
		purple_conv_chat_set_id(PURPLE_CONV_CHAT(conv), chat_number++);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node))
	{
/*		group = (PurpleGroup *) node;
		msg = skype_send_message("CHAT CREATE");
		sscanf(msg, "CHAT %s ", chat_id);
		g_free(msg);
		
		conv = serv_got_joined_chat(purple_account_get_connection(purple_buddy_get_account(buddy)), chat_number, chat_id);
		purple_conversation_set_data(conv, "chat_id", g_strdup(chat_id));
		purple_conv_chat_set_id(PURPLE_CONV_CHAT(conv), chat_number++);
		
		//loop through all the child nodes of the group
		
		//add buddy to the chat
		
		//add self to the chat
*/	}
}

static void
skype_chat_invite(PurpleConnection *gc, int id, const char *msg, const char *who)
{
	PurpleConversation *conv;
	gchar *chat_id;

	conv = purple_find_chat(gc, id);
	chat_id = (gchar *)g_hash_table_lookup(conv->data, "chat_id");

	skype_send_message_nowait("ALTER CHAT %s ADDMEMBERS %s", chat_id, who);
	purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), who, NULL, PURPLE_CBFLAGS_NONE, TRUE);
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
	char *stripped;
	
	stripped = purple_markup_strip_html(message);
	conv = purple_find_chat(gc, id);
	skype_debug_info("skype", "chat_send; conv: %d, conv->data: %d, id: %d\n", conv, conv->data, id);
	chat_id = (gchar *)g_hash_table_lookup(conv->data, "chat_id");
	skype_debug_info("skype", "chat_id: %s\n", chat_id);

	skype_send_message_nowait("CHATMESSAGE %s %s", chat_id, stripped);
/*#ifndef USE_SKYPE_SENT
	serv_got_chat_in(gc, id, purple_account_get_username(purple_connection_get_account(gc)), PURPLE_MESSAGE_SEND,
					message, time(NULL));
#endif*/
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

	serv_got_chat_in(gc, id, purple_account_get_username(purple_connection_get_account(gc)), PURPLE_MESSAGE_SYSTEM,
					skype_strdup_withhtml(g_strdup_printf(_("%s has changed the topic to: %s"), purple_account_get_username(purple_connection_get_account(gc)), topic)), time(NULL));
	purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), NULL, topic);
}

void
skype_join_chat(PurpleConnection *gc, GHashTable *data)
{
	static int chat_number = 2000;
	PurpleConversation *conv;
	
	gchar *chat_id = (gchar *)g_hash_table_lookup(data, "chat_id");
	if (chat_id == NULL)
	{
		return;
	}
	skype_send_message_nowait("ALTER CHAT %s JOIN", chat_id);
	conv = serv_got_joined_chat(gc, chat_number++, chat_id);
	purple_conversation_set_data(conv, "chat_id", g_strdup(chat_id));
}

gchar *
skype_get_chat_name(GHashTable *data)
{
	gchar *temp;

	if (data == NULL)
		return NULL;
	
	temp = g_hash_table_lookup(data, "chat_id");

	if (temp == NULL)
		return NULL;

	return g_strdup(temp);
}

gchar *
skype_cb_real_name(PurpleConnection *gc, int id, const char *who)
{
	PurpleBuddy *buddy;
	SkypeBuddy *sbuddy;
	
	buddy = purple_find_buddy(gc->account, who);
	if (buddy && buddy->proto_data)
	{
		sbuddy = buddy->proto_data;
		if (sbuddy->fullname)
			return g_strdup(sbuddy->fullname);
	} else if (buddy && buddy->server_alias)
		return g_strdup(buddy->server_alias);
	
	return skype_get_user_info(who, "FULLNAME");
}

GList *
skype_join_chat_info(PurpleConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Skype Name");
	pce->identifier = "chat_id";
	pce->required = TRUE;
	m = g_list_append(m, pce);
	
	return m;
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
skype_get_chatmessage_info(int message)
{
	skype_send_message_nowait("GET CHATMESSAGE %d TYPE", message);
	skype_send_message_nowait("GET CHATMESSAGE %d CHATNAME", message);
	skype_send_message_nowait("GET CHATMESSAGE %d BODY", message);
	skype_send_message_nowait("GET CHATMESSAGE %d FROM_HANDLE", message);
	skype_send_message_nowait("GET CHATMESSAGE %d USERS", message);
	skype_send_message_nowait("GET CHATMESSAGE %d LEAVEREASON", message);
	skype_send_message_nowait("GET CHATMESSAGE %d TIMESTAMP", message);
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

/* Like purple_strdup_withhtml, but escapes htmlentities too */
gchar *
skype_strdup_withhtml(const gchar *src)
{
	gulong destsize, i, j;
	gchar *dest;

	g_return_val_if_fail(src != NULL, NULL);

	/* New length is (length of src) + (number of \n's * 3) + (number of &'s * 5) + (number of <'s * 4) + (number of >'s *4) + (number of "'s * 6) - (number of \r's) + 1 */
	destsize = 1;
	for (i = 0; src[i] != '\0'; i++)
	{
		if (src[i] == '\n' || src[i] == '<' || src[i] == '>')
			destsize += 4;
		else if (src[i] == '&')
			destsize += 5;
		else if (src[i] == '"')
			destsize += 6;
		else if (src[i] != '\r')
			destsize++;
	}

	dest = g_malloc(destsize);

	/* Copy stuff, ignoring \r's, because they are dumb */
	for (i = 0, j = 0; src[i] != '\0'; i++) {
		if (src[i] == '\n') {
			strcpy(&dest[j], "<BR>");
			j += 4;
		} else if (src[i] == '<') {
			strcpy(&dest[j], "&lt;");
			j += 4;
		} else if (src[i] == '>') {
			strcpy(&dest[j], "&gt;");
			j += 4;
		} else if (src[i] == '&') {
			strcpy(&dest[j], "&amp;");
			j += 5;
		} else if (src[i] == '"') {
			strcpy(&dest[j], "&quot;");
			j += 6;
		} else if (src[i] != '\r')
			dest[j++] = src[i];
	}

	dest[destsize-1] = '\0';

	return dest;
}

static void
skype_display_skype_credit(PurplePluginAction *action)
{
	gchar *temp, *currency;
	double balance;
	
	temp = skype_send_message("GET PROFILE PSTN_BALANCE");
	balance = atol(&temp[21]);
	g_free(temp);
	balance = balance / 100;
	
	temp = skype_send_message("GET PROFILE PSTN_BALANCE_CURRENCY");
	currency = g_strdup(&temp[30]);
	g_free(temp);

	temp = g_strdup_printf("%s %.2f", currency, balance);

	skype_debug_info("skype", "Balance: '%s'\n", temp);
	purple_notify_info(this_plugin, _("Skype Balance"), _("Your current Skype credit balance is:"), temp);
	g_free(temp);
	g_free(currency);
}

static void
skype_open_sms_im(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	SkypeBuddy *sbuddy;
	PurpleConversation *conv;
	gchar *mobile_number = NULL;

	if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
		return;
	
	buddy = (PurpleBuddy *)node;
	sbuddy = (SkypeBuddy *)buddy->proto_data;

	if (buddy->name[0] == '+')
		mobile_number = buddy->name;
	else if (sbuddy && sbuddy->phone_mobile)
		mobile_number = sbuddy->phone_mobile;
	
	
	//Open a conversation window
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, mobile_number, buddy->account);
	if (conv == NULL)
	{
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, buddy->account, mobile_number);
	}
	//store the fact that it's an SMS convo in the conversation
	purple_conversation_set_data(conv, "sms_msg", TRUE);
}

int
skype_send_sms(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	gchar *sms_reply;
	PurpleConversation *conv;
	guint sms_number;
	gchar *stripped;

	if (who[0] != '+')
		return -1;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, purple_connection_get_account(gc));
	if (purple_conversation_get_data(conv, "sms_msg") == NULL)
		return -1;

	stripped = purple_markup_strip_html(message);

	//CREATE SMS OUTGOING +number
	sms_reply = skype_send_message("CREATE SMS OUTGOING %s", who);
	sscanf(sms_reply, "SMS %d ", &sms_number);
	g_free(sms_reply);
	skype_debug_info("skype", "SMS number: %d\n", sms_number);
	//if the sms price is > 0, output that to the window
	//SET SMS {sms number} BODY {body}
	skype_send_message_nowait("SET SMS %d BODY %s", sms_number, stripped);
	//ALTER SMS {sms number} SEND
	skype_send_message_nowait("ALTER SMS %d SEND", sms_number);
	//delete the sms number from the conversation
	return 1;
}


static void
dump_hash_table(gchar *key, gchar *value, gpointer data)
{
	printf("'%s' = '%s'\n", key, value);
}

/* copied from oscar.c to be libpurple 2.1 compatible */
static PurpleAccount *
find_acct(const char *prpl, const char *acct_id)
{
	PurpleAccount *acct = NULL;

	/* If we have a specific acct, use it */
	if (acct_id) {
		acct = purple_accounts_find(acct_id, prpl);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(prpl, purple_account_get_protocol_id(l->data))
					&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}

	return acct;
}

static gboolean
skype_uri_handler(const char *proto, const char *cmd, GHashTable *params)
{
	PurpleAccount *acct;
	gchar *temp;
	
	//only deal with skype: uri's
	if (!g_str_equal(proto, "skype"))
		return FALSE;
		
	/*uri's:
	
		skype:						//does nothing
		skype:{buddynames}			//open im with {buddynames}
		skype:{buddynames}?chat		//open multi-user chat with {buddynames}
		skype:?chat&blob={blob id}	//open public multi-user chat with the blob id of {blob id}
		skype:?chat&id={chat id}	//open multi-user chat with the id of {chat id}
		skype:{buddynames}?call		//call {buddynames}
		skype:{buddyname}?add		//add user to buddy list 
		
		//not sure about these ones
		skype:{buddynames}?userinfo	//get buddy's info?
		skype:{buddynames}?voicemail	//send a voice mail message?
		skype:{buddynames}?sendfile	//send a file?
		*/
	acct = find_acct(purple_plugin_get_id(this_plugin), g_hash_table_lookup(params, "account"));	


	//lets have a look at the hash table!
	skype_debug_info("skype", "dumping uri handler hashtable\n");
	g_hash_table_foreach(params, (GHFunc)dump_hash_table, NULL);
	
	if (g_hash_table_lookup(params, "chat"))
	{
		if (!strlen(cmd))
		{
			//probably a public multi-user chat?
			if ((temp = g_hash_table_lookup(params, "blob")))
			{
				temp = skype_send_message("CHAT CREATEUSINGBLOB %s", temp);
				if (strlen(temp))
				{
					g_free(temp);
					return FALSE;
				}
				*strchr(strchr(temp, ' '), ' ') = '\0';
				skype_send_message("ALTER %s JOIN", temp);
				g_free(temp);
				return TRUE;
			}
			else if ((temp = g_hash_table_lookup(params, "id")))
			{
				skype_send_message_nowait("ALTER CHAT %s JOIN", temp);
				return TRUE;
			}
		}
	} else if (g_hash_table_lookup(params, "add"))
	{
		purple_blist_request_add_buddy(skype_get_account(NULL), cmd, NULL, g_hash_table_lookup(params, "displayname"));
		return TRUE;
	} else if (g_hash_table_lookup(params, "call"))
	{
		skype_send_message_nowait("CALL %s", cmd);
		return TRUE;
	} else if (g_hash_table_lookup(params, "userinfo"))
	{
		skype_get_info(NULL, cmd);
		return TRUE;
	} else if (g_hash_table_lookup(params, "voicemail") || g_hash_table_lookup(params, "sendfile"))
	{
		//not sure?
	} else if (strlen(cmd)) {
		//there'll be a bunch of usernames, seperated by comma
		skype_send_message_nowait("CHAT CREATE %s", cmd);
		return TRUE;
	}
	
	//we don't know how to handle this
	return FALSE;
}

#ifdef USE_FARSIGHT
/*
Skype info from developer.skype.com and forum.skype.com:
Audio:
Audio format 

File: WAV PCM
Sockets: raw PCM samples
16 KHz mono, 16 bit
The 16-bit samples are stored as 2's-complement signed integers, ranging from -32768 to 32767.
there must be a call in progress when these API Audio calls are made to Skype to define the ports.
big-endian and it uses NO headers when using the Port form of the Audio API.

ALTER CALL <id> SET_INPUT SOUNDCARD="default" | PORT="port_no" | FILE="FILE_LOCATION" 

This enables you to set a port or a wav file as a source of your voice, instead of a microphone. 

ALTER CALL <id> SET_OUTPUT SOUNDCARD="default" | PORT="port_no" | FILE="FILE_LOCATION" 

Redirects incoming transmission to a port or a wav file.

With SET INPUT Skype acts like a Server, meaning, it waits to receive Audio Data from your application, so it does NOT act like a client. It will Open a Port and wait for your application to send Audio Data on the port defined, this is what a server does, a web server waits for a request on Port 80 for example.

With SET OUTPUT Skype acts like a Client, meaning, it sends data to your application, your application is waiting for Skype to send Audio data, which means your application acts as a listener ("Server").


Video:
SET VIDE0_IN [<device_name>]

Have to sniff for video window/object, very platform dependant

*/

//called by the UI to say, please start a media call
PurpleMedia *
skype_media_initiate(PurpleConnection *gc, const char *who, PurpleMediaStreamType type)
{
	gchar *temp;
	gchar *callnumber_string;

	//FarsightSession *fs = farsight_session_factory_make("rtp");
	//if (!fs) {
	//	skype_debug_error("jabber", "Farsight's rtp plugin not installed");
	//	return NULL;
	//}
	//FarsightStream *audio_stream = farsight_session_create_stream(fs, FARSIGHT_MEDIA_TYPE_AUDIO, FARSIGHT_STREAM_DIRECTION_BOTH);
	//FarsightStream *video_stream = farsight_session_create_stream(fs, FARSIGHT_MEDIA_TYPE_VIDEO, FARSIGHT_STREAM_DIRECTION_BOTH);
	
	//Use skype's own audio/video stuff for now
	PurpleMedia *media = purple_media_manager_create_media(purple_media_manager_get(), gc, who, NULL, NULL);
	
	/*farsight_stream_set_source(audio_stream, purple_media_get_audio_src(media));
	farsight_stream_set_sink(audio_stream, purple_media_get_audio_sink(media));
	farsight_stream_set_source(video_stream, purple_media_get_video_src(media));
	farsight_stream_set_sink(video_stream, purple_media_get_video_sink(media));
	
	g_signal_connect_swapped(G_OBJECT(media), "accepted", G_CALLBACK(google_send_call_accept), callnumber_string);
	g_signal_connect_swapped(G_OBJECT(media), "reject", G_CALLBACK(google_send_call_reject), callnumber_string);*/
	
	temp = skype_send_message("CALL %s", who);
	if (!temp || !strlen(temp))
	{
		g_free(temp);
		return NULL;
	}
	callnumber_string = g_new(gchar, 10+1);
	sscanf(temp, "CALL %s ", &callnumber_string);
	
	g_signal_connect_swapped(G_OBJECT(media), "hangup", G_CALLBACK(google_send_call_end), callnumber_string);
	g_signal_connect_swapped(G_OBJECT(media), "got-hangup", G_CALLBACK(google_send_call_end), callnumber_string);	
	
	return media;
}

//called when the user accepts an incomming call from the ui
static void 
skype_send_call_accept(char *callnumber_string)
{
	char *temp;
	
	if (!callnumber_string || !strlen(callnumber_string))
		return;
	temp = skype_send_message("ALTER CALL %s ANSWER", callnumber_string);
	if (!temp || strlen(temp) == 0)
	{
		//there was an error, hang up the the call
		return skype_handle_call_got_ended(callnumber_string);
	}
}

//called when the user rejects an incomming call from the ui
static void 
skype_send_call_reject(char *callnumber_string)
{
	if (!callnumber_string || !strlen(callnumber_string))
		return;
	skype_send_message_nowait("ALTER CALL %s END HANGUP", callnumber_string);
}

//called when the user ends a call from the ui
static void 
skype_send_call_end(char *callnumber_string)
{
	if (!callnumber_string || !strlen(callnumber_string))
		return;
	skype_send_message_nowait("ALTER CALL %s HANGUP", callnumber_string);
}

int
skype_find_media(PurpleMedia *media, const char *who)
{
	const char *screenname = purple_media_get_screenname(media);
	return strcmp(screenname, who);
}

//our call to someone else got ended
static void
skype_handle_call_got_ended(char *callnumber_string)
{
	char *temp;
	char *who;
	PurpleMediaManager *manager;
	PurpleMedia *media;
	GList glist_temp;
	
	temp = skype_send_message("GET CALL %s PARTNER_HANDLE", callnumber_string);
	if (!temp || !strlen(temp))
		return;
	
	who = g_strdup(&temp[21+strlen(callnumber_string)]);
	g_free(temp);
	
	manager = purple_media_manager_get();
	
	glist_temp = g_list_find_custom(manager->priv->medias, who, skype_find_media);
	if (!glist_temp || !glist_temp->data)
		return;
	
	media = glist_temp->data;
	purple_media_got_hangup(media);
}

//there's an incoming call... deal with it
static void
skype_handle_incoming_call(PurpleConnection *gc, char *callnumber_string)
{
	PurpleMedia *media;
	temp = skype_send_message("GET CALL %s PARTNER_HANDLE", callnumber_string);
	if (!temp || !strlen(temp))
		return;
	
	who = g_strdup(&temp[21+strlen(callnumber_string)]);
	g_free(temp);
	
	media = purple_media_manager_create_media(purple_media_manager_get(), gc, who, NULL, NULL);
	
	g_signal_connect_swapped(G_OBJECT(media), "accepted", G_CALLBACK(skype_send_call_accept), callnumber_string);
	g_signal_connect_swapped(G_OBJECT(media), "reject", G_CALLBACK(skype_send_call_reject), callnumber_string);
	g_signal_connect_swapped(G_OBJECT(media), "hangup", G_CALLBACK(skype_send_call_end), callnumber_string);
	
	purple_media_ready(media);
}
#endif

