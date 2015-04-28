/*
 * SkypeWeb Plugin for libpurple/Pidgin
 * Copyright (c) 2014-2015 Eion Robb    
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
		skypeweb_get_self_details(sa);
		
		if (sa->authcheck_timeout) 
			purple_timeout_remove(sa->authcheck_timeout);
		skypeweb_check_authrequests(sa);
		sa->authcheck_timeout = purple_timeout_add_seconds(120, (GSourceFunc)skypeweb_check_authrequests, sa);
		
		skypeweb_get_friend_list(sa);
		skypeweb_poll(sa);
		
		skype_web_get_offline_history(sa);
		
		skypeweb_set_status(sa->account, purple_account_get_active_status(sa->account));
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

	if (sbuddy && sbuddy->mood && *(sbuddy->mood))
	{
		return g_markup_printf_escaped("%s", sbuddy->mood);
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
	
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, NULL, FALSE, FALSE, FALSE);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, "Online", _("Online"), TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY, "Idle", _("Away"), TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_EXTENDED_AWAY, "Away", _("Not Available"), TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_UNAVAILABLE, "Busy", _("Do Not Disturb"), TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_INVISIBLE, "Hidden", _("Invisible"), TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_OFFLINE, "Offline", _("Offline"), TRUE, TRUE, FALSE, "message", "Mood", purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	
	return types;
}


static GList *
skypeweb_chat_info(PurpleConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Skype Name");
	pce->identifier = "chatname";
	pce->required = TRUE;
	m = g_list_append(m, pce);
	
	/*pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Password");
	pce->identifier = "password";
	pce->required = FALSE;
	m = g_list_append(m, pce);*/
	
	return m;
}

static GHashTable *
skypeweb_chat_info_defaults(PurpleConnection *gc, const char *chatname)
{
	GHashTable *defaults;
	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	if (chatname != NULL)
	{
		g_hash_table_insert(defaults, "chatname", g_strdup(chatname));
	}
	return defaults;
}

static gchar *
skypeweb_get_chat_name(GHashTable *data)
{
	gchar *temp;

	if (data == NULL)
		return NULL;
	
	temp = g_hash_table_lookup(data, "chatname");

	if (temp == NULL)
		return NULL;

	return g_strdup(temp);
}


static void
skypeweb_join_chat(PurpleConnection *pc, GHashTable *data)
{
	SkypeWebAccount *sa = pc->proto_data;
	gchar *chatname;
	gchar *post;
	GString *url;
	
	chatname = (gchar *)g_hash_table_lookup(data, "chatname");
	if (chatname == NULL)
	{
		return;
	}
	
	url = g_string_new("/v1/threads/");
	g_string_append_printf(url, "%s", purple_url_encode(chatname));
	g_string_append(url, "/members/");
	g_string_append_printf(url, "8:%s", purple_url_encode(sa->username));
	
	post = "{\"role\":\"User\"}";
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, url->str, post, NULL, NULL, TRUE);
	
	g_string_free(url, TRUE);
	
	skypeweb_get_conversation_history(sa, chatname);
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

static GList *
skypeweb_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	PurpleBuddy *buddy;
	SkypeWebAccount *sa = NULL;
	
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		buddy = (PurpleBuddy *)node;
		if (buddy->proto_data) {
			SkypeWebBuddy *sbuddy = (SkypeWebBuddy *)buddy->proto_data;
			sa = sbuddy->sa;
		} else {
			PurpleConnection *pc = purple_account_get_connection(buddy->account);
			sa = pc->proto_data;
		}
		
		if (sa != NULL) {
			act = purple_menu_action_new(_("Initiate _Chat"),
								PURPLE_CALLBACK(skypeweb_initiate_chat_from_node),
								sa, NULL);
			m = g_list_append(m, act);
		}
	}
	
	return m;
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

	pc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_NO_BGCOLOR | PURPLE_CONNECTION_NO_FONTSIZE;
	
	sa->username = g_strdup(account->username);
	sa->account = account;
	sa->pc = pc;
	sa->cookie_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	sa->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	sa->sent_messages_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	sa->waiting_conns = g_queue_new();
	sa->messages_host = g_strdup(SKYPEWEB_DEFAULT_MESSAGES_HOST);
	
	if(strchr(sa->username, '@')) {
		//Has an email address for a username, probably a microsoft account?
		skypeweb_begin_oauth_login(sa);
	} else {
		skypeweb_begin_web_login(sa);
	}
}

static void
skypeweb_close(PurpleConnection *pc)
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
	
	g_free(sa->messages_host);
	g_free(sa->skype_token);
	g_free(sa->registration_token);
	g_free(sa->endpoint);
	g_free(sa->username);
	g_free(sa);
}

gboolean
skypeweb_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
}

/******************************************************************************/
/* Plugin functions */
/******************************************************************************/

static gboolean
skypeweb_uri_handler(const char *proto, const char *cmd, GHashTable *params)
{
	PurpleAccount *account;
	PurpleConnection *pc;
	
	if (!g_str_equal(proto, "skype"))
		return FALSE;
		
	/*skype uri's:
	
		skype:						//does nothing
		skype:{buddyname}			//open im with {buddyname}
		skype:{buddynames}?chat		//open multi-user chat with {buddynames}
		skype:?chat&blob={blob id}	//open public multi-user chat with the blob id of {blob id}
		skype:?chat&id={chat id}	//open multi-user chat with the id of {chat id}
		skype:{buddyname}?add		//add user to buddy list 
		skype:{buddyname}?userinfo	//get buddy's info
		
		skype:{buddynames}?call		//call {buddynames}
		skype:{buddyname}?voicemail	//send a voice mail message
		skype:{buddyname}?sendfile	//send a file
		*/
	
	account = find_acct(SKYPEWEB_PLUGIN_ID, g_hash_table_lookup(params, "account"));
	pc = purple_account_get_connection(account);
	
	if (g_hash_table_lookup(params, "chat")) {
		if (cmd && *cmd) {
			//there'll be a bunch of usernames, seperated by semi-colon
			if (strchr(cmd, ';')) {
				gchar **users = g_strsplit_set(cmd, ";", -1);
				skypeweb_initiate_chat(pc->proto_data, users[0]);
				//TODO the other users
				g_strfreev(users);
			} else {
				PurpleConversation *conv;
				conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, cmd, account);
				if (!conv) {
					conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, cmd);
				}
				purple_conversation_present(conv);
			}
		} else {
			//probably a public multi-user chat?
			GHashTable *chatinfo = NULL;
			if (g_hash_table_lookup(params, "id")) {
				chatinfo = skypeweb_chat_info_defaults(pc, g_hash_table_lookup(params, "id"));
			} else if (g_hash_table_lookup(params, "blob")) {
				chatinfo = skypeweb_chat_info_defaults(pc, g_hash_table_lookup(params, "blob"));
			}
			
			if (chatinfo != NULL) {
				skypeweb_join_chat(pc, chatinfo);
				g_hash_table_destroy(chatinfo);
			}
		}
	} else if (g_hash_table_lookup(params, "add")) {
		purple_blist_request_add_buddy(account, cmd, "Skype", g_hash_table_lookup(params, "displayname"));
		return TRUE;
	} else if (g_hash_table_lookup(params, "call")) {
		
	} else if (g_hash_table_lookup(params, "userinfo")) {
		skypeweb_get_info(pc, cmd);
		return TRUE;
	} else if (g_hash_table_lookup(params, "voicemail")) {
		
	} else if (g_hash_table_lookup(params, "sendfile")) {
		
	} else if (strlen(cmd)) {
		//supposed to be the same as call?
	}
	
	//we don't know how to handle this
	return FALSE;
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static GList *
skypeweb_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Search for friends..."),
			skypeweb_search_users);
	m = g_list_append(m, act);

	return m;
}

static void
plugin_init(PurplePlugin *plugin)
{
	PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;
	
	option = purple_account_option_bool_new("", "", FALSE);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	
	/*
	//leave
	purple_cmd_register("leave", "", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						plugin->info->id, skypeweb_cmd_leave,
						_("leave [room]:  Leave the chat"), NULL);
	//topic
	purple_cmd_register("topic", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						plugin->info->id, skypeweb_cmd_topic,
						_("topic [&lt;new topic&gt;]:  View or change the topic"),
						NULL);
	//call, as in call person
	//kick
	purple_cmd_register("kick", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY,
						plugin->info->id, skypeweb_cmd_kick,
						_("kick &lt;user&gt; [room]:  Kick a user from the room."),
						NULL);
	//kickban
	purple_cmd_register("kickban", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY,
						plugin->info->id, skypeweb_cmd_kickban,
						_("kickban &lt;user&gt; [room]:  Kick and ban a user from the room."),
						NULL);
	//setrole
	purple_cmd_register("setrole", "ss", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY,
						plugin->info->id, skypeweb_cmd_setrole,
						_("setrole &lt;user&gt; &lt;MASTER | USER | ADMIN&gt;:  Change the role of a user."),
						NULL);
	*/
	
	purple_signal_connect(purple_get_core(), "uri-handler", plugin, PURPLE_CALLBACK(skypeweb_uri_handler), NULL);
}

static PurplePluginProtocolInfo prpl_info = {
#if PURPLE_VERSION_CHECK(3, 0, 0)
	sizeof(PurplePluginProtocolInfo),	/* struct_size */
#endif

	/* options */
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_INVITE_MESSAGE /*| OPT_PROTO_IM_IMAGE*/,

	NULL,                         /* user_splits */
	NULL,                         /* protocol_options */
	{"jpeg", 0, 0, 96, 96, 0, PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
	skypeweb_list_icon,           /* list_icon */
	skypeweb_list_emblem,         /* list_emblems */
	skypeweb_status_text,         /* status_text */
	skypeweb_tooltip_text,        /* tooltip_text */
	skypeweb_status_types,        /* status_types */
	skypeweb_node_menu,           /* blist_node_menu */
	skypeweb_chat_info,           /* chat_info */
	skypeweb_chat_info_defaults,  /* chat_info_defaults */
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
	skypeweb_buddy_block,         /* add_deny */
	NULL,                         /* rem_permit */
	skypeweb_buddy_unblock,       /* rem_deny */
	NULL,                         /* set_permit_deny */
	skypeweb_join_chat,           /* join_chat */
	NULL,                         /* reject chat invite */
	skypeweb_get_chat_name,       /* get_chat_name */
	skypeweb_chat_invite,         /* chat_invite */
	NULL,//skypeweb_chat_fake_leave,     /* chat_leave */
	NULL,                         /* chat_whisper */
	skypeweb_chat_send,           /* chat_send */
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
	skypeweb_offline_message,     /* offline_message */
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
