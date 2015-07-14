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
 
#include "skypeweb_messages.h"
#include "skypeweb_util.h"
#include "skypeweb_connection.h"
#include "skypeweb_contacts.h"



static gchar *
skypeweb_meify(const gchar *message, gint skypeemoteoffset)
{
	guint len;
	len = strlen(message);
	
	if (skypeemoteoffset <= 0 || skypeemoteoffset >= len)
		return g_strdup(message);
	
	return g_strconcat("/me ", message + skypeemoteoffset, NULL);
}

static void
process_userpresence_resource(SkypeWebAccount *sa, JsonObject *resource)
{
	const gchar *selfLink = json_object_get_string_member(resource, "selfLink");
	const gchar *status = json_object_get_string_member(resource, "status");
	const gchar *from;
	
	from = skypeweb_contact_url_to_name(selfLink);
	g_return_if_fail(from);
	
	//TODO not need me
	if (!purple_blist_find_buddy(sa->account, from))
	{
		PurpleGroup *group = purple_blist_find_group("Skype");
		if (!group)
		{
			group = purple_group_new("Skype");
			purple_blist_add_group(group, NULL);
		}
		purple_blist_add_buddy(purple_buddy_new(sa->account, from, NULL), NULL, group, NULL);
	}
	
	purple_protocol_got_user_status(sa->account, from, status, NULL);
	purple_protocol_got_user_idle(sa->account, from, g_str_equal(status, "Idle"), 0);
}

static void
process_message_resource(SkypeWebAccount *sa, JsonObject *resource)
{
	const gchar *clientmessageid = NULL;
	const gchar *skypeeditedid = NULL;
	const gchar *messagetype = json_object_get_string_member(resource, "messagetype");
	const gchar *from = json_object_get_string_member(resource, "from");
	const gchar *content = NULL;
	const gchar *composetime = json_object_get_string_member(resource, "composetime");
	const gchar *conversationLink = json_object_get_string_member(resource, "conversationLink");
	time_t composetimestamp = purple_str_to_time(composetime, TRUE, NULL, NULL, NULL);
	gchar **messagetype_parts;
	PurpleConversation *conv = NULL;
	gchar *convname = NULL;
	
	messagetype_parts = g_strsplit(messagetype, "/", -1);
	
	if (json_object_has_member(resource, "clientmessageid"))
		clientmessageid = json_object_get_string_member(resource, "clientmessageid");
	
	if (clientmessageid && *clientmessageid && g_hash_table_remove(sa->sent_messages_hash, clientmessageid)) {
		// We sent this message from here already
		g_strfreev(messagetype_parts);
		return;
	}
	
	if (json_object_has_member(resource, "skypeeditedid"))
		skypeeditedid = json_object_get_string_member(resource, "skypeeditedid");
	if (json_object_has_member(resource, "content"))
		content = json_object_get_string_member(resource, "content");
	
	if (strstr(conversationLink, "/19:")) {
		// This is a Thread/Group chat message
		const gchar *chatname, *topic;
		PurpleChatConversation *chatconv;
		
		chatname = skypeweb_thread_url_to_name(conversationLink);
		convname = g_strdup(chatname);
		chatconv = purple_conversations_find_chat_with_account(chatname, sa->account);
		if (!chatconv) {
			chatconv = purple_serv_got_joined_chat(sa->pc, g_str_hash(chatname), chatname);
			purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "chatname", g_strdup(chatname));
			
			topic = json_object_get_string_member(resource, "threadtopic");
			purple_chat_conversation_set_topic(chatconv, NULL, topic);
			
			skypeweb_get_conversation_history(sa, chatname);
			skypeweb_get_thread_users(sa, chatname);
		}
		conv = PURPLE_CONVERSATION(chatconv);
		
		if ((g_str_equal(messagetype, "RichText") || g_str_equal(messagetype, "Text")) && content && *content) {
			gchar *html;
			gint64 skypeemoteoffset = 0;
			
			if (json_object_has_member(resource, "skypeemoteoffset"))
				skypeemoteoffset = atoi(json_object_get_string_member(resource, "skypeemoteoffset"));
			
			from = skypeweb_contact_url_to_name(from);
			g_return_if_fail(from);
			
			//TODO if (skypeeditedid && *skypeeditedid) { ... }
			
			if (g_str_equal(messagetype, "Text")) {
				gchar *temp = skypeweb_meify(content, skypeemoteoffset);
				html = purple_markup_escape_text(temp, -1);
				g_free(temp);
			} else {
				html = skypeweb_meify(content, skypeemoteoffset);
			}
			purple_serv_got_chat_in(sa->pc, g_str_hash(chatname), from, g_str_equal(sa->username, from) ? PURPLE_MESSAGE_SEND : PURPLE_MESSAGE_RECV, html, composetimestamp);
					
			g_free(html);
		} else if (g_str_equal(messagetype, "ThreadActivity/AddMember")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			PurpleXmlNode *target;
			for(target = purple_xmlnode_get_child(blob, "target"); target;
				target = purple_xmlnode_get_next_twin(target))
			{
				gchar *user = purple_xmlnode_get_data(target);
				if (!purple_chat_conversation_find_user(chatconv, &user[2]))
					purple_chat_conversation_add_user(chatconv, &user[2], NULL, PURPLE_CHAT_USER_NONE, TRUE);
				g_free(user);
			}
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "ThreadActivity/DeleteMember")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			PurpleXmlNode *target;
			for(target = purple_xmlnode_get_child(blob, "target"); target;
				target = purple_xmlnode_get_next_twin(target))
			{
				gchar *user = purple_xmlnode_get_data(target);
				if (g_str_equal(sa->username, &user[2]))
					purple_chat_conversation_leave(chatconv);
				purple_chat_conversation_remove_user(chatconv, &user[2], NULL);
				g_free(user);
			}
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "ThreadActivity/TopicUpdate")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			gchar *initiator = purple_xmlnode_get_data(purple_xmlnode_get_child(blob, "initiator"));
			gchar *value = purple_xmlnode_get_data(purple_xmlnode_get_child(blob, "value"));
			
			purple_chat_conversation_set_topic(chatconv, &initiator[2], value);
			purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_TOPIC);
				
			g_free(initiator);
			g_free(value);
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "ThreadActivity/RoleUpdate")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			PurpleXmlNode *target;
			PurpleChatUser *cb;
			
			for(target = purple_xmlnode_get_child(blob, "target"); target;
				target = purple_xmlnode_get_next_twin(target))
			{
				gchar *user = purple_xmlnode_get_data(purple_xmlnode_get_child(target, "id"));
				gchar *role = purple_xmlnode_get_data(purple_xmlnode_get_child(target, "role"));
				PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE;
				
				if (role && *role) {
					if (g_str_equal(role, "Admin")) {
						cbflags = PURPLE_CHAT_USER_OP;
					} else if (g_str_equal(role, "User")) {
						cbflags = PURPLE_CHAT_USER_VOICE;
					}
				}
				#if !PURPLE_VERSION_CHECK(3, 0, 0)
					purple_conv_chat_user_set_flags(chatconv, &user[2], cbflags);
				#else
					cb = purple_chat_conversation_find_user(chatconv, &user[2]);
					purple_chat_user_set_flags(cb, cbflags);
				#endif
				
				g_free(user);
				g_free(role);
			} 
			
			purple_xmlnode_free(blob);
		} else {
			purple_debug_warning("skypeweb", "Unhandled thread message resource messagetype '%s'\n", messagetype);
		}
		
	} else {
		// This is a One-to-one/IM message
		convname = g_strconcat("8:", skypeweb_contact_url_to_name(conversationLink), NULL);
		from = skypeweb_contact_url_to_name(from);
		g_return_if_fail(from);
		
		if (g_str_equal(messagetype_parts[0], "Control")) {
			if (g_str_equal(messagetype_parts[1], "ClearTyping")) {
				purple_serv_got_typing(sa->pc, from, 7, PURPLE_IM_NOT_TYPING);
			} else if (g_str_equal(messagetype_parts[1], "Typing")) {
				purple_serv_got_typing(sa->pc, from, 7, PURPLE_IM_TYPING);
			}
		} else if ((g_str_equal(messagetype, "RichText") || g_str_equal(messagetype, "Text")) && content && *content) {
			gchar *html;
			gint64 skypeemoteoffset = 0;
			PurpleIMConversation *imconv;
			
			if (json_object_has_member(resource, "skypeemoteoffset"))
				skypeemoteoffset = atoi(json_object_get_string_member(resource, "skypeemoteoffset"));
			
			//TODO if (skypeeditedid && *skypeeditedid) { ... }
			
			if (g_str_equal(messagetype, "Text")) {
				gchar *temp = skypeweb_meify(content, skypeemoteoffset);
				html = purple_markup_escape_text(temp, -1);
				g_free(temp);
			} else {
				html = skypeweb_meify(content, skypeemoteoffset);
			}
			if (g_str_equal(sa->username, from)) {
				from = skypeweb_contact_url_to_name(conversationLink);
				if (from != NULL && !g_str_has_prefix(html, "?OTR")) {
					imconv = purple_conversations_find_im_with_account(from, sa->account);
					if (imconv == NULL)
					{
						imconv = purple_im_conversation_new(sa->account, from);
					}
					conv = PURPLE_CONVERSATION(imconv);
					purple_conversation_write(conv, from, html, PURPLE_MESSAGE_SEND, composetimestamp);
				}
			} else {
				purple_serv_got_im(sa->pc, from, html, PURPLE_MESSAGE_RECV, composetimestamp);
				
				imconv = purple_conversations_find_im_with_account(from, sa->account);
				conv = PURPLE_CONVERSATION(imconv);
			}
			g_free(html);
		} else if (g_str_equal(messagetype, "RichText/UriObject")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			const gchar *uri = purple_xmlnode_get_attrib(blob, "url_thumbnail");
			PurpleIMConversation *imconv;
			
			if (g_str_equal(sa->username, from)) {
				from = skypeweb_contact_url_to_name(conversationLink);
			}
			if (from != NULL) {
				imconv = purple_conversations_find_im_with_account(from, sa->account);
				if (imconv == NULL)
				{
					imconv = purple_im_conversation_new(sa->account, from);
				}
				
				conv = PURPLE_CONVERSATION(imconv);
				skypeweb_download_uri_to_conv(sa, uri, conv);
			}
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "Event/SkypeVideoMessage")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			const gchar *sid = purple_xmlnode_get_attrib(blob, "sid");
			PurpleIMConversation *imconv;
			
			if (g_str_equal(sa->username, from)) {
				from = skypeweb_contact_url_to_name(conversationLink);
			}
			if (from != NULL) {
				imconv = purple_conversations_find_im_with_account(from, sa->account);
				if (imconv == NULL)
				{
					imconv = purple_im_conversation_new(sa->account, from);
				}
				
				conv = PURPLE_CONVERSATION(imconv);
				//skypeweb_download_video_message(sa, sid, conv); //TODO
				purple_serv_got_im(sa->pc, from, content, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, composetimestamp);
			}
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "Event/Call")) {
			PurpleXmlNode *partlist = purple_xmlnode_from_str(content, -1);
			const gchar *partlisttype = purple_xmlnode_get_attrib(partlist, "type");
			const gchar *message = NULL;
			PurpleIMConversation *imconv;
			gboolean incoming = TRUE;
			
			if (g_str_equal(sa->username, from)) {
				incoming = FALSE;
				from = skypeweb_contact_url_to_name(conversationLink);
			}
			if (partlisttype && from != NULL) {
				imconv = purple_conversations_find_im_with_account(from, sa->account);
				if (imconv == NULL)
				{
					imconv = purple_im_conversation_new(sa->account, from);
				}
				
				conv = PURPLE_CONVERSATION(imconv);
				if (g_str_equal(partlisttype, "started")) {
					message = _("Call started");
				} else if (g_str_equal(partlisttype, "ended")) {
					PurpleXmlNode *part;
					gint duration_int = -1;
					
					for(part = purple_xmlnode_get_child(partlist, "part");
						part;
						part = purple_xmlnode_get_next_twin(part))
					{
						const gchar *identity = purple_xmlnode_get_attrib(part, "identity");
						if (identity && g_str_equal(sa->username, identity)) {
							PurpleXmlNode *duration = purple_xmlnode_get_child(part, "duration");
							if (duration != NULL) {
								gchar *duration_str;
								duration_str = purple_xmlnode_get_data(duration);
								duration_int = atoi(duration_str);
								break;
							}
						}
					}
					if (duration_int < 0) {
						message = _("Call missed");
					} else {
						//TODO report how long the call was
						message = _("Call ended");
					}
				}
				purple_serv_got_im(sa->pc, from, message, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, composetimestamp);
			}
			purple_xmlnode_free(partlist);
		} else {
			purple_debug_warning("skypeweb", "Unhandled message resource messagetype '%s'\n", messagetype);
		}
	}
	
	if (conv != NULL) {
		const gchar *id = json_object_get_string_member(resource, "id");
		
		g_free(purple_conversation_get_data(conv, "last_skypeweb_id"));
		
		if (purple_conversation_has_focus(conv)) {
			// Mark message as seen straight away
			gchar *post, *url;
			
			url = g_strdup_printf("/v1/users/ME/conversations/%s/properties?name=consumptionhorizon", purple_url_encode(convname));
			post = g_strdup_printf("{\"consumptionhorizon\":\"%s;%" G_GINT64_FORMAT ";%s\"}", id, skypeweb_get_js_time(), id);
			
			skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, url, post, NULL, NULL, TRUE);
			
			g_free(post);
			g_free(url);
			
			purple_conversation_set_data(conv, "last_skypeweb_id", NULL);
		} else {
			purple_conversation_set_data(conv, "last_skypeweb_id", g_strdup(id));
		}
	}
	
	g_free(convname);
	g_strfreev(messagetype_parts);
	
	if (composetimestamp > purple_account_get_int(sa->account, "last_message_timestamp", 0))
		purple_account_set_int(sa->account, "last_message_timestamp", composetimestamp);
}

static void
process_conversation_resource(SkypeWebAccount *sa, JsonObject *resource)
{
	const gchar *id = json_object_get_string_member(resource, "id");
	JsonObject *threadProperties;
	
	if (json_object_has_member(resource, "threadProperties")) {
		threadProperties = json_object_get_object_member(resource, "threadProperties");
	}
}

static void
process_thread_resource(SkypeWebAccount *sa, JsonObject *resource)
{
	
}

static void
process_endpointpresence_resource(SkypeWebAccount *sa, JsonObject *resource)
{
	JsonObject *publicInfo = json_object_get_object_member(resource, "publicInfo");
	if (publicInfo != NULL) {
		const gchar *typ_str = json_object_get_string_member(publicInfo, "typ");
		const gchar *skypeNameVersion = json_object_get_string_member(publicInfo, "skypeNameVersion");
		
		if (typ_str && *typ_str) {
			if (g_str_equal(typ_str, "website")) {
			
			} else {
				gint typ = atoi(typ_str);
				switch(typ) {
					case 17: //Android
						break;
					case 16: //iOS
						break;
					case 12: //WinRT/Metro
						break;
					case 15: //Winphone
						break;
					case 13: //OSX
						break;
					case 11: //Windows
						break;
					case 14: //Linux
						break;
					case 10: //XBox ? skypeNameVersion 11/1.8.0.1006
						break;
					case 1:  //SkypeWeb
						break;
					default:
						purple_debug_warning("skypeweb", "Unknown typ %d: %s\n", typ, skypeNameVersion);
						break;
				}
			}
		}
	}
}

gboolean
skypeweb_timeout(gpointer userdata)
{
	SkypeWebAccount *sa = userdata;
	skypeweb_poll(sa);
	
	// If no response within 3 minutes, assume connection lost and try again
	purple_timeout_remove(sa->watchdog_timeout);
	sa->watchdog_timeout = purple_timeout_add_seconds(3 * 60, skypeweb_timeout, sa);
	
	return FALSE;
}

static void
skypeweb_poll_cb(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonArray *messages = NULL;
	gint index, length;
	JsonObject *obj = NULL;
	
	if (node != NULL && json_node_get_node_type(node) == JSON_NODE_OBJECT)
		obj = json_node_get_object(node);
	
	if (obj != NULL) {
		if (json_object_has_member(obj, "eventMessages"))
			messages = json_object_get_array_member(obj, "eventMessages");
		
		if (messages != NULL) {
			length = json_array_get_length(messages);
			for(index = length - 1; index >= 0; index--)
			{
				JsonObject *message = json_array_get_object_element(messages, index);
				const gchar *resourceType = json_object_get_string_member(message, "resourceType");
				JsonObject *resource = json_object_get_object_member(message, "resource");
				
				if (g_str_equal(resourceType, "NewMessage"))
				{
					process_message_resource(sa, resource);
				} else if (g_str_equal(resourceType, "UserPresence"))
				{
					process_userpresence_resource(sa, resource);
				} else if (g_str_equal(resourceType, "EndpointPresence"))
				{
					process_endpointpresence_resource(sa, resource);
				} else if (g_str_equal(resourceType, "ConversationUpdate"))
				{
					process_conversation_resource(sa, resource);
				} else if (g_str_equal(resourceType, "ThreadUpdate"))
				{
					process_thread_resource(sa, resource);
				}
			}
		} else if (json_object_has_member(obj, "errorCode")) {
			gint64 errorCode = json_object_get_int_member(obj, "errorCode");
			
			if (errorCode == 729) {
				// "You must create an endpoint before performing this operation"
				// Dammit, Jim; I'm a programmer, not a surgeon!
				skypeweb_get_registration_token(sa);
				return;
			}
		}
		
		//TODO record id of highest recieved id to make sure we dont process the same id twice
	}
	
	sa->poll_timeout = purple_timeout_add_seconds(1, skypeweb_timeout, sa);
			
}

void
skypeweb_poll(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, "/v1/users/ME/endpoints/SELF/subscriptions/0/poll", NULL, skypeweb_poll_cb, NULL, TRUE);
}

void
skypeweb_mark_conv_seen(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	if (type == PURPLE_CONVERSATION_UPDATE_UNSEEN) {
		gchar *last_skypeweb_id = purple_conversation_get_data(conv, "last_skypeweb_id");
		
		if (last_skypeweb_id && *last_skypeweb_id) {
			PurpleAccount *account = purple_conversation_get_account(conv);
			SkypeWebAccount *sa = purple_connection_get_protocol_data(purple_account_get_connection(account));
			gchar *post, *url, *convname;
			
			if (PURPLE_IS_IM_CONVERSATION(conv)) {
				convname = g_strconcat("8:", purple_conversation_get_name(conv), NULL);
			} else {
				convname = g_strdup(purple_conversation_get_data(conv, "chatname"));
			}
			
			url = g_strdup_printf("/v1/users/ME/conversations/%s/properties?name=consumptionhorizon", purple_url_encode(convname));
			post = g_strdup_printf("{\"consumptionhorizon\":\"%s;%" G_GINT64_FORMAT ";%s\"}", last_skypeweb_id, skypeweb_get_js_time(), last_skypeweb_id);
			
			skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, url, post, NULL, NULL, TRUE);
			
			g_free(convname);
			g_free(post);
			g_free(url);
			
			g_free(last_skypeweb_id);
			purple_conversation_set_data(conv, "last_skypeweb_id", NULL);
		}
	}
}

static void
skypeweb_got_thread_users(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	PurpleChatConversation *chatconv;
	gchar *chatname = user_data;
	JsonObject *response;
	JsonArray *members;
	gint length, index;
	
	chatconv = purple_conversations_find_chat_with_account(chatname, sa->account);
	if (chatconv == NULL)
		return;
	purple_chat_conversation_clear_users(chatconv);
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
	response = json_node_get_object(node);
	
	members = json_object_get_array_member(response, "members");
	length = json_array_get_length(members);
	for(index = length - 1; index >= 0; index--)
	{
		JsonObject *member = json_array_get_object_element(members, index);
		const gchar *userLink = json_object_get_string_member(member, "userLink");
		const gchar *role = json_object_get_string_member(member, "role");
		const gchar *username = skypeweb_contact_url_to_name(userLink);
		PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE;
		
		if (role && *role) {
			if (g_str_equal(role, "Admin")) {
				cbflags = PURPLE_CHAT_USER_OP;
			} else if (g_str_equal(role, "User")) {
				cbflags = PURPLE_CHAT_USER_VOICE;
			}
		}
		
		if (username == NULL && json_object_has_member(member, "linkedMri")) {
			username = skypeweb_contact_url_to_name(json_object_get_string_member(member, "linkedMri"));
		}
		if (username != NULL) {
			purple_chat_conversation_add_user(chatconv, username, NULL, cbflags, FALSE);
		}
	}
}

void
skypeweb_get_thread_users(SkypeWebAccount *sa, const gchar *convname)
{
	gchar *url;
	url = g_strdup_printf("/v1/threads/%s?view=msnp24Equivalent", purple_url_encode(convname));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, sa->messages_host, url, NULL, skypeweb_got_thread_users, g_strdup(convname), TRUE);
	
	g_free(url);
}

static void
skypeweb_got_conv_history(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	gint since = GPOINTER_TO_INT(user_data);
	JsonObject *obj;
	JsonArray *messages;
	gint index, length;
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
	obj = json_node_get_object(node);
	
	messages = json_object_get_array_member(obj, "messages");
	length = json_array_get_length(messages);
	for(index = length - 1; index >= 0; index--)
	{
		JsonObject *message = json_array_get_object_element(messages, index);
		const gchar *composetime = json_object_get_string_member(message, "composetime");
		gint composetimestamp = (gint) purple_str_to_time(composetime, TRUE, NULL, NULL, NULL);
		
		if (composetimestamp > since) {
			process_message_resource(sa, message);
		}
	}
}

void
skypeweb_get_conversation_history_since(SkypeWebAccount *sa, const gchar *convname, gint since)
{
	gchar *url;
	url = g_strdup_printf("/v1/users/ME/conversations/%s/messages?startTime=%d000&pageSize=30&view=msnp24Equivalent&targetType=Passport|Skype|Lync|Thread", purple_url_encode(convname), since);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, sa->messages_host, url, NULL, skypeweb_got_conv_history, GINT_TO_POINTER(since), TRUE);
	
	g_free(url);
}

void
skypeweb_get_conversation_history(SkypeWebAccount *sa, const gchar *convname)
{
	skypeweb_get_conversation_history_since(sa, convname, 0);
}

static void
skypeweb_got_all_convs(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	gint since = GPOINTER_TO_INT(user_data);
	JsonObject *obj;
	JsonArray *conversations;
	gint index, length;
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
	obj = json_node_get_object(node);
	
	conversations = json_object_get_array_member(obj, "conversations");
	length = json_array_get_length(conversations);
	for(index = 0; index < length; index++) {
		JsonObject *conversation = json_array_get_object_element(conversations, index);
		const gchar *id = json_object_get_string_member(conversation, "id");
		JsonObject *lastMessage = json_object_get_object_member(conversation, "lastMessage");
		if (lastMessage != NULL && json_object_has_member(lastMessage, "composetime")) {
			const gchar *composetime = json_object_get_string_member(lastMessage, "composetime");
			gint composetimestamp = (gint) purple_str_to_time(composetime, TRUE, NULL, NULL, NULL);
			
			if (composetimestamp > since) {
				skypeweb_get_conversation_history_since(sa, id, since);
			}
		}
	}
}

void
skypeweb_get_all_conversations_since(SkypeWebAccount *sa, gint since)
{
	gchar *url;
	url = g_strdup_printf("/v1/users/ME/conversations?startTime=%d000&pageSize=100&view=msnp24Equivalent&targetType=Passport|Skype|Lync|Thread", since);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, sa->messages_host, url, NULL, skypeweb_got_all_convs, GINT_TO_POINTER(since), TRUE);
	
	g_free(url);
}

void
skype_web_get_offline_history(SkypeWebAccount *sa)
{
	skypeweb_get_all_conversations_since(sa, purple_account_get_int(sa->account, "last_message_timestamp", ((gint) time(NULL))));
}


static void
skypeweb_got_roomlist_threads(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	PurpleRoomlist *roomlist = user_data;
	JsonObject *obj;
	JsonArray *conversations;
	gint index, length;
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
	obj = json_node_get_object(node);
	
	conversations = json_object_get_array_member(obj, "conversations");
	length = json_array_get_length(conversations);
	for(index = 0; index < length; index++) {
		JsonObject *conversation = json_array_get_object_element(conversations, index);
		const gchar *id = json_object_get_string_member(conversation, "id");
		PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, id, NULL);
		
		purple_roomlist_room_add_field(roomlist, room, id);
		if (json_object_has_member(conversation, "threadProperties")) {
			JsonObject *threadProperties = json_object_get_object_member(conversation, "threadProperties");
			if (threadProperties != NULL) {
				const gchar *topic = json_object_get_string_member(threadProperties, "topic");
				purple_roomlist_room_add_field(roomlist, room, topic);
			}
		}
		purple_roomlist_room_add(roomlist, room);
	}
	
	purple_roomlist_set_in_progress(roomlist, FALSE);
}

PurpleRoomlist *
skypeweb_roomlist_get_list(PurpleConnection *pc)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	const gchar *url = "/v1/users/ME/conversations?startTime=0&pageSize=100&view=msnp24Equivalent&targetType=Thread";
	PurpleRoomlist *roomlist;
	GList *fields = NULL;
	PurpleRoomlistField *f;
	
	roomlist = purple_roomlist_new(sa->account);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("ID"), "chatname", TRUE);
	fields = g_list_append(fields, f);

	//f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Users"), "users", FALSE);
	//fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	purple_roomlist_set_fields(roomlist, fields);
	purple_roomlist_set_in_progress(roomlist, TRUE);

	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, sa->messages_host, url, NULL, skypeweb_got_roomlist_threads, roomlist, FALSE);
	
	return roomlist;
}

void
skypeweb_unsubscribe_from_contact_status(SkypeWebAccount *sa, const gchar *who)
{
	const gchar *contacts_url = "/v1/users/ME/contacts";
	gchar *url;
	
	url = g_strconcat(contacts_url, "/8:", purple_url_encode(who), NULL);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_DELETE | SKYPEWEB_METHOD_SSL, sa->messages_host, url, NULL, NULL, NULL, TRUE);
	
	g_free(url);
}

void
skypeweb_subscribe_to_contact_status(SkypeWebAccount *sa, GSList *contacts)
{
	const gchar *contacts_url = "/v1/users/ME/contacts";
	gchar *post;
	GSList *cur = contacts;
	JsonObject *obj;
	JsonArray *contacts_array;
	guint count = 0;
	
	if (contacts == NULL)
		return;
	
	obj = json_object_new();
	contacts_array = json_array_new();
	
	do {
		JsonObject *contact = json_object_new();
		gchar *id;
		
		id = g_strconcat("8:", cur->data, NULL);
		json_object_set_string_member(contact, "id", id);
		json_array_add_object_element(contacts_array, contact);
		
		g_free(id);
		
		if (count++ >= 100) {
			// Send off the current batch and continue
			json_object_set_array_member(obj, "contacts", contacts_array);
			post = skypeweb_jsonobj_to_string(obj);

			skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, contacts_url, post, NULL, NULL, TRUE);
			
			g_free(post);
			json_object_unref(obj);
			
			obj = json_object_new();
			contacts_array = json_array_new();
			count = 0;
		}
	} while((cur = g_slist_next(cur)));
	
	json_object_set_array_member(obj, "contacts", contacts_array);
	post = skypeweb_jsonobj_to_string(obj);

	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, contacts_url, post, NULL, NULL, TRUE);
	
	g_free(post);
	json_object_unref(obj);
}


static void
skypeweb_subscribe_cb(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{	
	skypeweb_do_all_the_things(sa);
}

static void
skypeweb_subscribe(SkypeWebAccount *sa)
{
	JsonObject *obj;
	JsonArray *interested;
	gchar *post;

	interested = json_array_new();
	json_array_add_string_element(interested, "/v1/users/ME/conversations/ALL/properties");
	json_array_add_string_element(interested, "/v1/users/ME/conversations/ALL/messages");
	json_array_add_string_element(interested, "/v1/users/ME/contacts/ALL");
	json_array_add_string_element(interested, "/v1/threads/ALL");
	
	obj = json_object_new();
	json_object_set_array_member(obj, "interestedResources", interested);
	json_object_set_string_member(obj, "template", "raw");
	json_object_set_string_member(obj, "channelType", "httpLongPoll");
	
	post = skypeweb_jsonobj_to_string(obj);

	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, "/v1/users/ME/endpoints/SELF/subscriptions", post, skypeweb_subscribe_cb, NULL, TRUE);
	
	g_free(post);
	json_object_unref(obj);
}

static void
skypeweb_got_registration_token(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	gchar *registration_token;
	gchar *endpointId;
	SkypeWebAccount *sa = user_data;
	gchar *new_messages_host;
	
	if (url_text == NULL) {
		url_text = url_data->webdata;
		len = url_data->data_len;
	}
	
	if (url_text == NULL) {
		if (purple_major_version == 2 && (
			purple_minor_version < 10 ||
			(purple_minor_version == 10 && purple_micro_version < 11))
			) {
			purple_connection_error (sa->pc,
									PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
									_("Your version of libpurple is too old.\nUpgrade to 2.10.11 or newer and try again."));
			return;
		}
	}
	
	new_messages_host = skypeweb_string_get_chunk(url_text, len, "Location: https://", "/");
	if (new_messages_host != NULL && !g_str_equal(sa->messages_host, new_messages_host)) {
		g_free(sa->messages_host);
		sa->messages_host = new_messages_host;
		
		// Your princess is in another castle
		purple_debug_info("skypeweb", "Messages host has changed to %s\n", sa->messages_host);
		
		skypeweb_get_registration_token(sa);
		return;
	}
	
	registration_token = skypeweb_string_get_chunk(url_text, len, "Set-RegistrationToken: ", ";");
	endpointId = skypeweb_string_get_chunk(url_text, len, "endpointId=", "\r\n");
	
	if (registration_token == NULL) {
		purple_connection_error (sa->pc,
								PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
								_("Failed getting Registration Token"));
		return;
	}
	//purple_debug_info("skypeweb", "New RegistrationToken is %s\n", registration_token);
	
	g_free(sa->registration_token); sa->registration_token = registration_token;
	g_free(sa->endpoint); sa->endpoint = endpointId;
	
	skypeweb_subscribe(sa);
}

void
skypeweb_get_registration_token(SkypeWebAccount *sa)
{
	gchar *messages_url;
	gchar *request;
	gchar *curtime;
	gchar *response;
	PurpleUtilFetchUrlData *requestdata;
	
	g_free(sa->registration_token); sa->registration_token = NULL;
	g_free(sa->endpoint); sa->endpoint = NULL;
	
	curtime = g_strdup_printf("%d", (int) time(NULL));
	response = skypeweb_hmac_sha256(curtime);
	
	messages_url = g_strdup_printf("https://%s/v1/users/ME/endpoints", sa->messages_host);
	
	request = g_strdup_printf("POST /v1/users/ME/endpoints HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: */*\r\n"
			"BehaviorOverride: redirectAs404\r\n"
			"LockAndKey: appId=" SKYPEWEB_LOCKANDKEY_APPID "; time=%s; lockAndKeyResponse=%s\r\n"
			"ClientInfo: os=Windows; osVer=8.1; proc=Win32; lcid=en-us; deviceType=1; country=n/a; clientName=" SKYPEWEB_CLIENTINFO_NAME "; clientVer=" SKYPEWEB_CLIENTINFO_VERSION "\r\n"
			"Host: %s\r\n"
			"Content-Type: application/json\r\n"
			"Authentication: skypetoken=%s\r\n"
			"Content-Length: 2\r\n\r\n{}",
			curtime, response, sa->messages_host, sa->skype_token);
	
	//purple_debug_info("skypeweb", "reg token request is %s\n", request);
	
	requestdata = purple_util_fetch_url_request(sa->account, messages_url, TRUE, NULL, FALSE, request, TRUE, 524288, skypeweb_got_registration_token, sa);
	requestdata->num_times_redirected = 10; /* Prevent following redirects */

	g_free(request);
	g_free(curtime);
	g_free(response);
	g_free(messages_url);
}





guint
skypeweb_send_typing(PurpleConnection *pc, const gchar *name, PurpleIMTypingState state)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *post, *url;
	JsonObject *obj;
	
	url = g_strdup_printf("/v1/users/ME/conversations/8:%s/messages", purple_url_encode(name));
	
	obj = json_object_new();
	json_object_set_int_member(obj, "clientmessageid", time(NULL));
	json_object_set_string_member(obj, "content", "");
	json_object_set_string_member(obj, "messagetype", state == PURPLE_IM_TYPING ? "Control/Typing" : "Control/ClearTyping");
	json_object_set_string_member(obj, "contenttype", "text");
	
	post = skypeweb_jsonobj_to_string(obj);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, url, post, NULL, NULL, TRUE);
	
	g_free(post);
	json_object_unref(obj);
	g_free(url);
	
	return 5;
}


static void
skypeweb_set_statusid(SkypeWebAccount *sa, const gchar *status)
{
	gchar *post;
	
	g_return_if_fail(status);
	
	post = g_strdup_printf("{\"status\":\"%s\"}", status);
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, "/v1/users/ME/presenceDocs/messagingService", post, NULL, NULL, TRUE);
	g_free(post);

	if (sa->endpoint) {
		gchar *url = g_strdup_printf("/v1/users/ME/endpoints/%s/presenceDocs/messagingService", purple_url_encode(sa->endpoint));
		post = "{\"id\":\"messagingService\", \"type\":\"EndpointPresenceDoc\", \"selfLink\":\"uri\", \"privateInfo\":{\"epname\":\"skype\"}, \"publicInfo\":{\"capabilities\":\"\", \"type\":1, \"typ\":1, \"skypeNameVersion\":\"" SKYPEWEB_CLIENTINFO_VERSION "/" SKYPEWEB_CLIENTINFO_NAME "\", \"nodeInfo\":\"xx\", \"version\":\"" SKYPEWEB_CLIENTINFO_VERSION "\"}}";
		skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, url, post, NULL, NULL, TRUE);
	}
}

void
skypeweb_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	
	skypeweb_set_statusid(sa, purple_status_get_id(status));
}

void
skypeweb_set_idle(PurpleConnection *pc, int time)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	
	if (time < 30) {
		skypeweb_set_statusid(sa, "Online");
	} else {
		skypeweb_set_statusid(sa, "Idle");
	}
}


static void
skypeweb_send_message(SkypeWebAccount *sa, const gchar *convname, const gchar *message)
{
	gchar *post, *url;
	JsonObject *obj;
	gint64 clientmessageid;
	gchar *clientmessageid_str;
	gchar *stripped;
	
	url = g_strdup_printf("/v1/users/ME/conversations/%s/messages", purple_url_encode(convname));
	
	clientmessageid = skypeweb_get_js_time();
	clientmessageid_str = g_strdup_printf("%" G_GINT64_FORMAT "", clientmessageid);
	
	// Some clients don't receive messages with <br>'s in them
	stripped = purple_strreplace(message, "<br>", "\r\n");
	
	obj = json_object_new();
	json_object_set_string_member(obj, "clientmessageid", clientmessageid_str);
	json_object_set_string_member(obj, "content", stripped);
	json_object_set_string_member(obj, "messagetype", "RichText");
	json_object_set_string_member(obj, "contenttype", "text");
	
	if (g_str_has_prefix(message, "/me ")) {
		json_object_set_string_member(obj, "skypeemoteoffset", "4"); //Why is this a string :(
	}
	
	post = skypeweb_jsonobj_to_string(obj);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, url, post, NULL, NULL, TRUE);
	
	g_free(post);
	json_object_unref(obj);
	g_free(url);
	g_free(stripped);
	
	g_hash_table_insert(sa->sent_messages_hash, clientmessageid_str, clientmessageid_str);
}


gint
skypeweb_chat_send(PurpleConnection *pc, gint id, const gchar *message, PurpleMessageFlags flags)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	
	PurpleChatConversation *chatconv;
	gchar* chatname;
	
	chatconv = purple_conversations_find_chat(pc, id);
	chatname = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "chatname");
	if (!chatname)
		return -1;

	skypeweb_send_message(sa, chatname, message);

	purple_serv_got_chat_in(pc, id, sa->username, PURPLE_MESSAGE_SEND, message, time(NULL));

	return 1;
}

gint
skypeweb_send_im(PurpleConnection *pc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *convname;
	
	convname = g_strconcat("8:", who, NULL);
	skypeweb_send_message(sa, convname, message);
	g_free(convname);
	
	return 1;
}


void
skypeweb_chat_invite(PurpleConnection *pc, int id, const char *message, const char *who)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	PurpleChatConversation *chatconv;
	gchar *chatname;
	gchar *post;
	GString *url;

	chatconv = purple_conversations_find_chat(pc, id);
	chatname = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "chatname");
	
	url = g_string_new("/v1/threads/");
	g_string_append_printf(url, "%s", purple_url_encode(chatname));
	g_string_append(url, "/members/");
	g_string_append_printf(url, "8:%s", purple_url_encode(who));
	
	post = "{\"role\":\"User\"}";
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, url->str, post, NULL, NULL, TRUE);
	
	g_string_free(url, TRUE);
}

void
skypeweb_chat_kick(PurpleConnection *pc, int id, const char *who)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	PurpleChatConversation *chatconv;
	gchar *chatname;
	gchar *post;
	GString *url;

	chatconv = purple_conversations_find_chat(pc, id);
	chatname = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "chatname");
	
	url = g_string_new("/v1/threads/");
	g_string_append_printf(url, "%s", purple_url_encode(chatname));
	g_string_append(url, "/members/");
	g_string_append_printf(url, "8:%s", purple_url_encode(who));
	
	post = "";
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_DELETE | SKYPEWEB_METHOD_SSL, sa->messages_host, url->str, post, NULL, NULL, TRUE);
	
	g_string_free(url, TRUE);
}

void
skypeweb_initiate_chat(SkypeWebAccount *sa, const gchar *who)
{
	JsonObject *obj, *contact;
	JsonArray *members;
	gchar *id, *post;
	
	obj = json_object_new();
	members = json_array_new();
	
	contact = json_object_new();
	id = g_strconcat("8:", who, NULL);
	json_object_set_string_member(contact, "id", id);
	json_object_set_string_member(contact, "role", "User");
	json_array_add_object_element(members, contact);
	g_free(id);
	
	contact = json_object_new();
	id = g_strconcat("8:", sa->username, NULL);
	json_object_set_string_member(contact, "id", id);
	json_object_set_string_member(contact, "role", "Admin");
	json_array_add_object_element(members, contact);
	g_free(id);
	
	json_object_set_array_member(obj, "members", members);
	post = skypeweb_jsonobj_to_string(obj);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, "/v1/threads", post, NULL, NULL, TRUE);

	g_free(post);
	json_object_unref(obj);
}

void
skypeweb_initiate_chat_from_node(PurpleBlistNode *node, gpointer userdata)
{
	if(PURPLE_IS_BUDDY(node))
	{
		PurpleBuddy *buddy = (PurpleBuddy *) node;
		SkypeWebAccount *sa;
		
		if (userdata) {
			sa = userdata;
		} else {
			PurpleConnection *pc = purple_account_get_connection(purple_buddy_get_account(buddy));
			sa = purple_connection_get_protocol_data(pc);
		}
		
		skypeweb_initiate_chat(sa, purple_buddy_get_name(buddy));
	}
}

void
skypeweb_chat_set_topic(PurpleConnection *pc, int id, const char *topic)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	PurpleChatConversation *chatconv;
	JsonObject *obj;
	gchar *chatname;
	gchar *post;
	GString *url;

	chatconv = purple_conversations_find_chat(pc, id);
	chatname = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "chatname");
	
	url = g_string_new("/v1/threads/");
	g_string_append_printf(url, "%s", purple_url_encode(chatname));
	g_string_append(url, "/properties?name=topic");
	
	obj = json_object_new();
	json_object_set_string_member(obj, "topic", topic);
	post = skypeweb_jsonobj_to_string(obj);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, url->str, post, NULL, NULL, TRUE);
	
	g_string_free(url, TRUE);
	g_free(post);
	json_object_unref(obj);
}

