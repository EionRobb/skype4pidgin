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
		purple_connection_set_state(sa->pc, PURPLE_CONNECTED);

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
	
	if (buddy != NULL) {
		const gchar *buddy_name = purple_buddy_get_name(buddy);
		if (buddy_name && SKYPEWEB_BUDDY_IS_MSN(buddy_name)) {
			return "msn";
		}
	}
	return "skype";
}

static gchar *
skypeweb_status_text(PurpleBuddy *buddy)
{
	SkypeWebBuddy *sbuddy = purple_buddy_get_protocol_data(buddy);

	if (sbuddy && sbuddy->mood && *(sbuddy->mood))
	{
		return g_markup_printf_escaped("%s", sbuddy->mood);
	}

	return NULL;
}

void
skypeweb_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	SkypeWebBuddy *sbuddy = purple_buddy_get_protocol_data(buddy);
	
	if (sbuddy)
	{
		PurplePresence *presence;
		PurpleStatus *status;
		SkypeWebBuddy *sbuddy = purple_buddy_get_protocol_data(buddy);

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
	//SkypeWebBuddy *sbuddy = purple_buddy_get_protocol_data(buddy);
	
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
	PurpleProtocolChatEntry *pce;

	pce = g_new0(PurpleProtocolChatEntry, 1);
	pce->label = _("Skype Name");
	pce->identifier = "chatname";
	pce->required = TRUE;
	m = g_list_append(m, pce);
	
	/*pce = g_new0(PurpleProtocolChatEntry, 1);
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
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *chatname;
	gchar *post;
	GString *url;
	PurpleChatConversation *chatconv;
	
	chatname = (gchar *)g_hash_table_lookup(data, "chatname");
	if (chatname == NULL)
	{
		return;
	}
	
	chatconv = purple_conversations_find_chat_with_account(chatname, sa->account);
	if (chatconv != NULL) {
		purple_conversation_present(PURPLE_CONVERSATION(chatconv));
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
	skypeweb_get_thread_users(sa, chatname);
	
	chatconv = purple_serv_got_joined_chat(pc, g_str_hash(chatname), chatname);
	purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "chatname", g_strdup(chatname));
	
	purple_conversation_present(PURPLE_CONVERSATION(chatconv));
}

static void
skypeweb_buddy_free(PurpleBuddy *buddy)
{
	SkypeWebBuddy *sbuddy = purple_buddy_get_protocol_data(buddy);
	if (sbuddy != NULL)
	{
		purple_buddy_set_protocol_data(buddy, NULL);

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
	
	if(PURPLE_IS_BUDDY(node))
	{
		buddy = PURPLE_BUDDY(node);
		if (purple_buddy_get_protocol_data(buddy)) {
			SkypeWebBuddy *sbuddy = purple_buddy_get_protocol_data(buddy);
			sa = sbuddy->sa;
		}
		if (sa == NULL) {
			PurpleConnection *pc = purple_account_get_connection(purple_buddy_get_account(buddy));
			sa = purple_connection_get_protocol_data(pc);
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
	PurpleConnectionFlags flags;
	
	purple_connection_set_protocol_data(pc, sa);
	
	if (!purple_ssl_is_supported()) {
		purple_connection_error (pc,
								PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
								_("Server requires TLS/SSL for login.  No TLS/SSL support found."));
		return;
	}

	flags = purple_connection_get_flags(pc);
	flags |= PURPLE_CONNECTION_FLAG_HTML | PURPLE_CONNECTION_FLAG_NO_BGCOLOR | PURPLE_CONNECTION_FLAG_NO_FONTSIZE;
	purple_connection_set_flags(pc, flags);
	
	sa->username = g_strdup(purple_account_get_username(account));
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
	
	purple_signal_connect(purple_conversations_get_handle(), "conversation-updated", pc, PURPLE_CALLBACK(skypeweb_mark_conv_seen), NULL);
}

static void
skypeweb_close(PurpleConnection *pc)
{
	SkypeWebAccount *sa;
	
	g_return_if_fail(pc != NULL);
	
	sa = purple_connection_get_protocol_data(pc);
	g_return_if_fail(sa != NULL);
	
	purple_timeout_remove(sa->authcheck_timeout);
	purple_timeout_remove(sa->poll_timeout);
	purple_timeout_remove(sa->watchdog_timeout);

	skypeweb_logout(sa);
	purple_signal_disconnect(purple_conversations_get_handle(), "conversation-updated", pc, PURPLE_CALLBACK(skypeweb_mark_conv_seen));
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

static PurpleCmdRet
skypeweb_cmd_list(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	purple_roomlist_show_with_account(purple_conversation_get_account(conv));
	
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
skypeweb_cmd_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *pc = NULL;
	int id = -1;
	SkypeWebAccount *sa;
	
	pc = purple_conversation_get_connection(conv);
	id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv));
	
	if (pc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	sa = purple_connection_get_protocol_data(pc);
	if (sa == NULL)
		return PURPLE_CMD_RET_FAILED;
	
	skypeweb_chat_kick(pc, id, sa->username);
	
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
skypeweb_cmd_kick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *pc = NULL;
	int id = -1;
	
	pc = purple_conversation_get_connection(conv);
	id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv));
	
	if (pc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	skypeweb_chat_kick(pc, id, args[0]);
	
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
skypeweb_cmd_invite(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *pc = NULL;
	int id = -1;
	
	pc = purple_conversation_get_connection(conv);	
	id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv));
	
	if (pc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	skypeweb_chat_invite(pc, id, NULL, args[0]);
	
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
skypeweb_cmd_topic(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *pc = NULL;
	PurpleChatConversation *chat;
	int id = -1;
	
	pc = purple_conversation_get_connection(conv);
	chat = PURPLE_CHAT_CONVERSATION(conv);
	id = purple_chat_conversation_get_id(chat);
	
	if (pc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;

	if (!args || !args[0]) {
		gchar *buf;
		const gchar *topic = purple_chat_conversation_get_topic(chat);

		if (topic) {
			gchar *tmp, *tmp2;
			tmp = g_markup_escape_text(topic, -1);
			tmp2 = purple_markup_linkify(tmp);
			buf = g_strdup_printf(_("current topic is: %s"), tmp2);
			g_free(tmp);
			g_free(tmp2);
		} else {
			buf = g_strdup(_("No topic is set"));
		}
		
		purple_conv_chat_write(chat, NULL, buf, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG, time(NULL));
		
		g_free(buf);
		return PURPLE_CMD_RET_OK;
	}
	
	skypeweb_chat_set_topic(pc, id, args ? args[0] : NULL);
	
	return PURPLE_CMD_RET_OK;
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
				skypeweb_initiate_chat(purple_connection_get_protocol_data(pc), users[0]);
				//TODO the other users
				g_strfreev(users);
			} else {
				PurpleIMConversation *imconv;
				imconv = purple_conversations_find_im_with_account(cmd, account);
				if (!imconv) {
					imconv = purple_im_conversation_new(account, cmd);
				}
				purple_conversation_present(PURPLE_CONVERSATION(imconv));
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

#if PURPLE_VERSION_CHECK(3, 0, 0)
	static PurpleProtocol *skypeweb_protocol;
#endif


static gboolean
plugin_load(PurplePlugin *plugin
#if PURPLE_VERSION_CHECK(3, 0, 0)
, GError **error
#endif
)
{
	
#if PURPLE_VERSION_CHECK(3, 0, 0)
	skypeweb_protocol = purple_protocols_add(SKYPEWEB_TYPE_PROTOCOL, error);
	if (!skypeweb_protocol)
		return FALSE;
#endif
	
	
	//leave
	purple_cmd_register("leave", "", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						plugin->info->id, skypeweb_cmd_leave,
						_("leave:  Leave the group chat"), NULL);
	//kick
	purple_cmd_register("kick", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY,
						plugin->info->id, skypeweb_cmd_kick,
						_("kick &lt;user&gt;:  Kick a user from the group chat."),
						NULL);
	//add
	purple_cmd_register("add", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY,
						plugin->info->id, skypeweb_cmd_invite,
						_("add &lt;user&gt;:  Add a user to the group chat."),
						NULL);
	//topic
	purple_cmd_register("topic", "s", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						plugin->info->id, skypeweb_cmd_topic,
						_("topic [&lt;new topic&gt;]:  View or change the topic"),
						NULL);
	/*
	//call, as in call person
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
	
	purple_cmd_register("list", "", PURPLE_CMD_P_PRPL, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_IM,
						plugin->info->id, skypeweb_cmd_list,
						_("list: Display a list of multi-chat group chats you are in."),
						NULL);
	
	purple_signal_connect(purple_get_core(), "uri-handler", plugin, PURPLE_CALLBACK(skypeweb_uri_handler), NULL);
	
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin
#if PURPLE_VERSION_CHECK(3, 0, 0)
, GError **error
#endif
)
{
#if PURPLE_VERSION_CHECK(3, 0, 0)
	if (!purple_protocols_remove(skypeweb_protocol, error))
		return FALSE;
#endif
	
	return TRUE;
}

static GList *
skypeweb_actions(
#if !PURPLE_VERSION_CHECK(3, 0, 0)
PurplePlugin *plugin, gpointer context
#else
PurpleConnection *pc
#endif
)
{
	GList *m = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Search for friends..."),
			skypeweb_search_users);
	m = g_list_append(m, act);

	return m;
}

#if !PURPLE_VERSION_CHECK(2, 8, 0)
#	define OPT_PROTO_INVITE_MESSAGE 0x00000800
#endif

#if !PURPLE_VERSION_CHECK(3, 0, 0)
G_MODULE_EXPORT gboolean 
purple_init_plugin(PurplePlugin *plugin)
{
	PurplePluginInfo *info = g_new0(PurplePluginInfo, 1);
	PurplePluginProtocolInfo *prpl_info = g_new0(PurplePluginProtocolInfo, 1);
#endif

#if PURPLE_VERSION_CHECK(3, 0, 0)
static void 
skypeweb_protocol_init(PurpleProtocol *prpl_info) 
{
	PurpleProtocol *info = prpl_info;
#endif
	PurpleAccountOption *option;
	PurpleBuddyIconSpec icon_spec = {"jpeg", 0, 0, 96, 96, 0, PURPLE_ICON_SCALE_DISPLAY};

	//PurpleProtocol
	info->id = SKYPEWEB_PLUGIN_ID;
	info->name = "Skype (HTTP)";
	prpl_info->options = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_INVITE_MESSAGE /*| OPT_PROTO_IM_IMAGE*/;
	option = purple_account_option_bool_new("", "", FALSE);
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	prpl_info->icon_spec = icon_spec;
#else
	prpl_info->account_options = g_list_append(prpl_info->account_options, option);
	prpl_info->icon_spec = &icon_spec;
#endif
	
#if PURPLE_VERSION_CHECK(3, 0, 0)
}

static void 
skypeweb_protocol_class_init(PurpleProtocolClass *prpl_info) 
{
#endif
	//PurpleProtocolClass
	prpl_info->login = skypeweb_login;
	prpl_info->close = skypeweb_close;
	prpl_info->status_types = skypeweb_status_types;
	prpl_info->list_icon = skypeweb_list_icon;
#if PURPLE_VERSION_CHECK(3, 0, 0)
}

static void 
skypeweb_protocol_client_iface_init(PurpleProtocolClientIface *prpl_info) 
{
	PurpleProtocolClientIface *info = prpl_info;
#endif
	
	//PurpleProtocolClientIface
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	info->actions = skypeweb_actions;
#else
	info->get_actions = skypeweb_actions;
#endif
	prpl_info->list_emblem = skypeweb_list_emblem;
	prpl_info->status_text = skypeweb_status_text;
	prpl_info->tooltip_text = skypeweb_tooltip_text;
	prpl_info->blist_node_menu = skypeweb_node_menu;
	prpl_info->buddy_free = skypeweb_buddy_free;
	prpl_info->normalize = purple_normalize_nocase;
	prpl_info->offline_message = skypeweb_offline_message;
	prpl_info->get_account_text_table = NULL; // skypeweb_get_account_text_table;
#if PURPLE_VERSION_CHECK(3, 0, 0)
}

static void 
skypeweb_protocol_server_iface_init(PurpleProtocolServerIface *prpl_info) 
{
#endif
	
	//PurpleProtocolServerIface
	prpl_info->get_info = skypeweb_get_info;
	prpl_info->set_status = skypeweb_set_status;
	prpl_info->set_idle = skypeweb_set_idle;
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	prpl_info->add_buddy = skypeweb_add_buddy;
#else
	prpl_info->add_buddy = skypeweb_add_buddy_with_invite;
#endif
	prpl_info->remove_buddy = skypeweb_buddy_remove;
	prpl_info->group_buddy = skypeweb_fake_group_buddy;
	prpl_info->rename_group = skypeweb_fake_group_rename;
#if PURPLE_VERSION_CHECK(3, 0, 0)
}

static void 
skypeweb_protocol_im_iface_init(PurpleProtocolIMIface *prpl_info) 
{
#endif
	
	//PurpleProtocolIMIface
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	prpl_info->send_im = skypeweb_send_im;
#else
	prpl_info->send = skypeweb_send_im;
#endif
	prpl_info->send_typing = skypeweb_send_typing;
#if PURPLE_VERSION_CHECK(3, 0, 0)
}

static void 
skypeweb_protocol_chat_iface_init(PurpleProtocolChatIface *prpl_info) 
{
#endif
	
	//PurpleProtocolChatIface
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	prpl_info->chat_info = skypeweb_chat_info;
	prpl_info->chat_info_defaults = skypeweb_chat_info_defaults;
	prpl_info->join_chat = skypeweb_join_chat;
	prpl_info->get_chat_name = skypeweb_get_chat_name;
	prpl_info->chat_invite = skypeweb_chat_invite;
	prpl_info->chat_leave =	NULL; //skypeweb_chat_fake_leave;
	prpl_info->chat_send = skypeweb_chat_send;
	prpl_info->set_chat_topic = skypeweb_chat_set_topic;
#else
	prpl_info->info = skypeweb_chat_info;
	prpl_info->info_defaults = skypeweb_chat_info_defaults;
	prpl_info->join = skypeweb_join_chat;
	prpl_info->get_name = skypeweb_get_chat_name;
	prpl_info->invite = skypeweb_chat_invite;
	prpl_info->leave =	NULL; //skypeweb_chat_fake_leave;
	prpl_info->send = skypeweb_chat_send;
	prpl_info->set_topic = skypeweb_chat_set_topic;
#endif
#if PURPLE_VERSION_CHECK(3, 0, 0)
}

static void 
skypeweb_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *prpl_info) 
{
#endif

	//PurpleProtocolPrivacyIface
	prpl_info->add_deny = skypeweb_buddy_block;
	prpl_info->rem_deny = skypeweb_buddy_unblock;
#if PURPLE_VERSION_CHECK(3, 0, 0)
}

static void 
skypeweb_protocol_roomlist_iface_init(PurpleProtocolRoomlistIface *prpl_info) 
{
#endif
	
	//PurpleProtocolRoomlistIface
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	prpl_info->roomlist_get_list = skypeweb_roomlist_get_list;
#else
	prpl_info->get_list = skypeweb_roomlist_get_list;
#endif
	
#if !PURPLE_VERSION_CHECK(3, 0, 0)
	// Plugin info
	info->magic = PURPLE_PLUGIN_MAGIC;
	info->major_version = PURPLE_MAJOR_VERSION;
	info->minor_version = PURPLE_MINOR_VERSION;
	info->type = PURPLE_PLUGIN_PROTOCOL;
	info->priority = PURPLE_PRIORITY_DEFAULT;
	info->version = SKYPEWEB_PLUGIN_VERSION;
	info->summary = N_("Skype for Web Protocol Plugin");
	info->description = N_("Skype for Web Protocol Plugin");
	info->author = "Eion Robb <eionrobb@gmail.com>";
	info->homepage = "http://github.com/EionRobb/skype4pidgin";
	info->load = plugin_load;
	info->unload = plugin_unload;
	info->extra_info = prpl_info;
	
	// Protocol info
	#if PURPLE_MINOR_VERSION >= 5
		prpl_info->struct_size = sizeof(PurplePluginProtocolInfo);
	#endif
	#if PURPLE_MINOR_VERSION >= 8
		prpl_info->add_buddy_with_invite = skypeweb_add_buddy_with_invite;
	#endif
	
	plugin->info = info;
	return purple_plugin_register(plugin);
#endif
	
}

#if PURPLE_VERSION_CHECK(3, 0, 0)


PURPLE_DEFINE_TYPE_EXTENDED(
	SkypeWebProtocol, skypeweb_protocol, PURPLE_TYPE_PROTOCOL, G_TYPE_FLAG_ABSTRACT,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  skypeweb_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  skypeweb_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  skypeweb_protocol_im_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  skypeweb_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  skypeweb_protocol_privacy_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE,
	                                  skypeweb_protocol_roomlist_iface_init)
);

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           SKYPEWEB_PLUGIN_ID,
		"name",         "SkypeWeb Protocol",
		"version",      SKYPEWEB_PLUGIN_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("SkypeWeb Protocol Plugin"),
		"description",  N_("SkypeWeb Protocol Plugin"),
		"website",      "http://github.com/EionRobb/skype4pidgin",
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}


PURPLE_PLUGIN_INIT(skypeweb, plugin_query, plugin_load, plugin_unload);
#endif

