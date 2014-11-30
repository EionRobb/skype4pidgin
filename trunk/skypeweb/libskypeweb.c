#include "libskypeweb.h"
#include "skypeweb_connection.h"
#include "skypeweb_contacts.h"
#include "skypeweb_login.h"
#include "skypeweb_messages.h"
#include "skypeweb_util.h"

void
skypeweb_do_all_the_things(SkypeWebAccount *sa)
{
	if (sa->registration_token) {
		if (sa->authcheck_timeout) 
			purple_timeout_remove(sa->authcheck_timeout);
		skypeweb_check_authrequests(sa);
		sa->authcheck_timeout = purple_timeout_add_seconds(120, (GSourceFunc)skypeweb_check_authrequests, sa);
		
		skypeweb_get_friend_list(sa);
		skypeweb_poll(sa);
	} else {
		//Too soon!
		skypeweb_get_registration_token(sa);
	}
}


/******************************************************************************/
/* PRPL functions */
/******************************************************************************/

static const char *
skypeweb_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "skype";
}

static gchar *
skypeweb_status_text(PurpleBuddy *buddy)
{
	SkypeWebBuddy *sbuddy = buddy->proto_data;

	if (sbuddy && (sbuddy->mood || sbuddy->rich_mood))
	{
		if (sbuddy->rich_mood)
		{
			return sbuddy->rich_mood;
		} else {
			return g_markup_printf_escaped("%s", sbuddy->mood);
		}
	}

	return NULL;
}

void
skypeweb_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	SkypeWebBuddy *sbuddy = buddy->proto_data;
	
	if (sbuddy)
	{
		PurplePresence *presence;
		PurpleStatus *status;
		SkypeWebBuddy *sbuddy = buddy->proto_data;

		presence = purple_buddy_get_presence(buddy);
		status = purple_presence_get_active_status(presence);
		purple_notify_user_info_add_pair_html(user_info, _("Status"), purple_status_get_name(status));
		if (sbuddy->mood && *sbuddy->mood)
			purple_notify_user_info_add_pair_html(user_info, _("Message"), sbuddy->mood);
			
		if (sbuddy->display_name && *sbuddy->display_name)
			purple_notify_user_info_add_pair_html(user_info, "Alias", sbuddy->display_name);
		if (sbuddy->fullname && *sbuddy->fullname)
			purple_notify_user_info_add_pair_html(user_info, "Full Name", sbuddy->fullname);
	}
}

const gchar *
skypeweb_list_emblem(PurpleBuddy *buddy)
{
	//SkypeWebBuddy *sbuddy = buddy->proto_data;
	
	return NULL;
}

GList *
skypeweb_status_types(PurpleAccount *account)
{
	GList *types = NULL;
	PurpleStatusType *status;
	
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, "Online", "Online", TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_OFFLINE, "Offline", "Offline", TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, "Idle", "Idle", FALSE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY, "Away", "Away", TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_EXTENDED_AWAY, "Hidden", "Hidden", TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	
	return types;
}


static void
skypeweb_buddy_free(PurpleBuddy *buddy)
{
	SkypeWebBuddy *sbuddy = buddy->proto_data;
	if (sbuddy != NULL)
	{
		buddy->proto_data = NULL;

		g_free(sbuddy->skypename);
		g_free(sbuddy->fullname);
		g_free(sbuddy->display_name);
		g_free(sbuddy->avatar_url);
		g_free(sbuddy->mood);
		g_free(sbuddy->rich_mood);
		
		g_free(sbuddy);
	}
}

void
skypeweb_fake_group_buddy(PurpleConnection *pc, const char *who, const char *old_group, const char *new_group)
{
	// Do nothing to stop the remove+add behaviour
}
void
skypeweb_fake_group_rename(PurpleConnection *pc, const char *old_name, PurpleGroup *group, GList *moved_buddies)
{
	// Do nothing to stop the remove+add behaviour
}


static void
skypeweb_login(PurpleAccount *account)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	SkypeWebAccount *sa = g_new0(SkypeWebAccount, 1);
	
	pc->proto_data = sa;
	
	if (!purple_ssl_is_supported()) {
		purple_connection_error (pc,
								PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
								_("Server requires TLS/SSL for login.  No TLS/SSL support found."));
		return;
	}

	pc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_NO_BGCOLOR | PURPLE_CONNECTION_NO_URLDESC | PURPLE_CONNECTION_NO_FONTSIZE | PURPLE_CONNECTION_NO_IMAGES;
	
	sa->account = account;
	sa->pc = pc;
	sa->cookie_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	sa->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	sa->sent_messages_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	sa->waiting_conns = g_queue_new();
	
	skypeweb_begin_web_login(sa);
}

static void skypeweb_close(PurpleConnection *pc)
{
	SkypeWebAccount *sa;
	
	g_return_if_fail(pc != NULL);
	g_return_if_fail(pc->proto_data != NULL);
	
	sa = pc->proto_data;
	
	purple_timeout_remove(sa->authcheck_timeout);
	purple_timeout_remove(sa->poll_timeout);
	purple_timeout_remove(sa->watchdog_timeout);

	skypeweb_logout(sa);
	
	purple_debug_info("skypeweb", "destroying %d waiting connections\n",
					  g_queue_get_length(sa->waiting_conns));
	
	while (!g_queue_is_empty(sa->waiting_conns))
		skypeweb_connection_destroy(g_queue_pop_tail(sa->waiting_conns));
	g_queue_free(sa->waiting_conns);
	
	purple_debug_info("skypeweb", "destroying %d incomplete connections\n",
			g_slist_length(sa->conns));

	while (sa->conns != NULL)
		skypeweb_connection_destroy(sa->conns->data);
		
	while (sa->dns_queries != NULL) {
		PurpleDnsQueryData *dns_query = sa->dns_queries->data;
		purple_debug_info("skypeweb", "canceling dns query for %s\n",
					purple_dnsquery_get_host(dns_query));
		sa->dns_queries = g_slist_remove(sa->dns_queries, dns_query);
		purple_dnsquery_destroy(dns_query);
	}
	
	g_hash_table_destroy(sa->sent_messages_hash);
	g_hash_table_destroy(sa->cookie_table);
	g_hash_table_destroy(sa->hostname_ip_cache);

	g_free(sa->skype_token);
	g_free(sa->registration_token);
	g_free(sa);
}

/******************************************************************************/
/* Plugin functions */
/******************************************************************************/

static gboolean plugin_load(PurplePlugin *plugin)
{
	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static GList *skypeweb_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Search for friends..."),
			skypeweb_search_users);
	m = g_list_append(m, act);

	return m;
}

static void plugin_init(PurplePlugin *plugin)
{
	/*PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;
	
	option = purple_account_option_bool_new(
		_("Always use HTTPS"),
		"always_use_https", FALSE);
	prpl_info->protocol_options = g_list_append(
		prpl_info->protocol_options, option);*/

}

static PurplePluginProtocolInfo prpl_info = {
#if PURPLE_VERSION_CHECK(3, 0, 0)
	sizeof(PurplePluginProtocolInfo),	/* struct_size */
#endif

	/* options */
	OPT_PROTO_CHAT_TOPIC,

	NULL,                         /* user_splits */
	NULL,                         /* protocol_options */
	/* NO_BUDDY_ICONS */          /* icon_spec */
	{"jpeg", 0, 0, 96, 96, 0, PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
	skypeweb_list_icon,           /* list_icon */
	skypeweb_list_emblem,         /* list_emblems */
	skypeweb_status_text,         /* status_text */
	skypeweb_tooltip_text,        /* tooltip_text */
	skypeweb_status_types,        /* status_types */
	NULL,//skypeweb_node_menu,           /* blist_node_menu */
	NULL,//skypeweb_chat_info,           /* chat_info */
	NULL,//skypeweb_chat_info_defaults,  /* chat_info_defaults */
	skypeweb_login,               /* login */
	skypeweb_close,               /* close */
	skypeweb_send_im,             /* send_im */
	NULL,                         /* set_info */
	skypeweb_send_typing,         /* send_typing */
	skypeweb_get_info,            /* get_info */
	skypeweb_set_status,          /* set_status */
	skypeweb_set_idle,            /* set_idle */
	NULL,                         /* change_passwd */
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	skypeweb_add_buddy,           /* add_buddy */
#else
	skypeweb_add_buddy_with_invite,
#endif
	NULL,                         /* add_buddies */
	skypeweb_buddy_remove,        /* remove_buddy */
	NULL,                         /* remove_buddies */
	NULL,                         /* add_permit */
	NULL,                         /* add_deny */
	NULL,                         /* rem_permit */
	NULL,                         /* rem_deny */
	NULL,                         /* set_permit_deny */
	NULL,//skypeweb_fake_join_chat,      /* join_chat */
	NULL,                         /* reject chat invite */
	NULL,//skypeweb_get_chat_name,       /* get_chat_name */
	NULL,                         /* chat_invite */
	NULL,//skypeweb_chat_fake_leave,     /* chat_leave */
	NULL,                         /* chat_whisper */
	NULL,//skypeweb_chat_send,           /* chat_send */
	NULL,                         /* keepalive */
	NULL,                         /* register_user */
	NULL,                         /* get_cb_info */
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	NULL,                         /* get_cb_away */
#endif
	NULL,                         /* alias_buddy */
	skypeweb_fake_group_buddy,    /* group_buddy */
	skypeweb_fake_group_rename,   /* rename_group */
	skypeweb_buddy_free,          /* buddy_free */
	NULL,                         /* convo_closed */
	purple_normalize_nocase,      /* normalize */
	NULL,                         /* set_buddy_icon */
	NULL,                         /* remove_group */
	NULL,                         /* get_cb_real_name */
	NULL,                         /* set_chat_topic */
	NULL,                         /* find_blist_chat */
	NULL,                         /* roomlist_get_list */
	NULL,                         /* roomlist_cancel */
	NULL,                         /* roomlist_expand_category */
	NULL,                         /* can_receive_file */
	NULL,                         /* send_file */
	NULL,                         /* new_xfer */
	NULL,                         /* offline_message */
	NULL,                         /* whiteboard_prpl_ops */
	NULL,                         /* send_raw */
	NULL,                         /* roomlist_room_serialize */
	NULL,                         /* unregister_user */
	NULL,                         /* send_attention */
	NULL,                         /* attention_types */
#if (PURPLE_MAJOR_VERSION == 2 && PURPLE_MINOR_VERSION >= 5) || PURPLE_MAJOR_VERSION > 2
#if PURPLE_MAJOR_VERSION == 2 && PURPLE_MINOR_VERSION >= 5
	sizeof(PurplePluginProtocolInfo), /* struct_size */
#endif
	NULL, // skypeweb_get_account_text_table, /* get_account_text_table */
	NULL,                         /* initiate_media */
	NULL,                         /* can_do_media */
	NULL,                         /* get_moods */
	NULL,                         /* set_public_alias */
	NULL                          /* get_public_alias */
#if PURPLE_MAJOR_VERSION == 2 && PURPLE_MINOR_VERSION >= 8
,	skypeweb_add_buddy_with_invite, /* add_buddy_with_invite */
	NULL                          /* add_buddies_with_invite */
#endif
#endif
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,				/* major_version */
	PURPLE_MINOR_VERSION, 				/* minor version */
	PURPLE_PLUGIN_PROTOCOL, 			/* type */
	NULL, 						/* ui_requirement */
	0, 						/* flags */
	NULL, 						/* dependencies */
	PURPLE_PRIORITY_DEFAULT, 			/* priority */
	SKYPEWEB_PLUGIN_ID,				/* id */
	"Skype (HTTP)", 					/* name */
	SKYPEWEB_PLUGIN_VERSION, 			/* version */
	N_("Skype for Web Protocol Plugin"), 		/* summary */
	N_("Skype for Web Protocol Plugin"), 		/* description */
	"Eion Robb <eionrobb@gmail.com>", 		/* author */
	"http://skype4pidgin.googlecode.com/",	/* homepage */
	plugin_load, 					/* load */
	plugin_unload, 					/* unload */
	NULL, 						/* destroy */
	NULL, 						/* ui_info */
	&prpl_info, 					/* extra_info */
	NULL, 						/* prefs_info */
	skypeweb_actions, 					/* actions */

							/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(skypeweb, plugin_init, info);
