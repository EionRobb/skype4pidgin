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
#include "skypeweb_login.h"

static gboolean
skypeweb_is_user_self(SkypeWebAccount *sa, const gchar *username) {
	if (!username || *username == 0) {
		return FALSE;
	}
	
	if (sa->username) {
		if (g_str_equal(username, sa->username)) {
			return TRUE;
		}
	}
	
	if (sa->primary_member_name) {
		if (g_str_equal(username, sa->primary_member_name)) {
			return TRUE;
		}
	}
	
	return !g_ascii_strcasecmp(username, purple_account_get_username(sa->account));
}

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
	// const gchar *capabilities = json_object_get_string_member(resource, "capabilities");
	// const gchar *lastSeenAt = json_object_get_string_member(resource, "lastSeenAt");
	const gchar *from;
	gboolean is_idle;
	
	from = skypeweb_contact_url_to_name(selfLink);
	g_return_if_fail(from);
	
	if (!purple_blist_find_buddy(sa->account, from))
	{
		PurpleGroup *group = purple_blist_find_group("Skype");
		if (!group)
		{
			group = purple_group_new("Skype");
			purple_blist_add_group(group, NULL);
		}
		
		if (skypeweb_is_user_self(sa, from)) {
			return;
		}
		
		purple_blist_add_buddy(purple_buddy_new(sa->account, from, NULL), NULL, group, NULL);
	}
	
	// if (g_str_equal(capabilities, "IsMobile")) {  //"Seamless | IsMobile"
		// purple_protocol_got_user_status(sa->account, from, "mobile", NULL);
	// }
	
	is_idle = purple_strequal(status, SKYPEWEB_STATUS_IDLE);
	if (!is_idle) {
		purple_protocol_got_user_status(sa->account, from, status, NULL);
	} else {
		purple_protocol_got_user_status(sa->account, from, SKYPEWEB_STATUS_ONLINE, NULL);
	}
	
	purple_protocol_got_user_idle(sa->account, from, is_idle, 0);
}

// static gboolean
// skypeweb_clear_typing_hack(PurpleChatUser *cb)
// {
	// PurpleChatUserFlags cbflags;
	
	// cbflags = purple_chat_user_get_flags(cb);
	// cbflags &= ~PURPLE_CHAT_USER_TYPING & ~PURPLE_CHAT_USER_VOICE;
	// purple_chat_user_set_flags(cb, cbflags);
	
	// return FALSE;
// }

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
	
	if (conversationLink && strstr(conversationLink, "/19:")) {
		// This is a Thread/Group chat message
		const gchar *chatname, *topic;
		PurpleChatConversation *chatconv;
		
		chatname = skypeweb_thread_url_to_name(conversationLink);
		convname = g_strdup(chatname);
		chatconv = purple_conversations_find_chat_with_account(chatname, sa->account);
		if (!chatconv) {
			chatconv = purple_serv_got_joined_chat(sa->pc, g_str_hash(chatname), chatname);
			purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "chatname", g_strdup(chatname));

			if (json_object_has_member(resource, "threadtopic")) {
				topic = json_object_get_string_member(resource, "threadtopic");
				purple_chat_conversation_set_topic(chatconv, NULL, topic);
			}
				
			skypeweb_get_conversation_history(sa, chatname);
			skypeweb_get_thread_users(sa, chatname);
		}
		conv = PURPLE_CONVERSATION(chatconv);
		
		if (g_str_equal(messagetype, "Control/Typing")) {
			PurpleChatUserFlags cbflags;
			PurpleChatUser *cb;

			from = skypeweb_contact_url_to_name(from);
			g_return_if_fail(from);

			// typing notification text, not personalized because of multiple "typing" events
			if (purple_account_get_bool(sa->account, "show-typing-as-text", FALSE)) {
				const gchar *message = g_strdup_printf("%s ...", N_("buddy typing"));
				const gchar *last_message = NULL;

				// get last message (first in GList)
				if (conv && g_list_length(purple_conversation_get_message_history(conv))) {
					PurpleMessage *last = g_list_nth_data(g_list_first(purple_conversation_get_message_history(conv)),0);
					last_message = g_strdup(purple_message_get_contents(last));
				}

				// add typing notification to chat
				if (last_message && !g_str_equal(last_message, message)) {
					PurpleMessage *msg = purple_message_new_system(message, PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_NOTIFY | PURPLE_MESSAGE_ACTIVE_ONLY);
					purple_message_set_time(msg, composetimestamp);
					purple_conversation_write_message(conv, msg);
					purple_message_destroy(msg);
				}
			}
			
			cb = purple_chat_conversation_find_user(chatconv, from);
			if (cb != NULL) {
				cbflags = purple_chat_user_get_flags(cb);
				
				cbflags |= PURPLE_CHAT_USER_TYPING;
				
				// typing notification icon
				if (purple_account_get_bool(sa->account, "show-typing-as-icon", FALSE)) {
					cbflags |= PURPLE_CHAT_USER_VOICE;
				}
				
				purple_chat_user_set_flags(cb, cbflags);
				
				//purple_timeout_add_seconds(7, skypeweb_clear_typing_hack, cb);
			}
			
		} else if ((g_str_equal(messagetype, "RichText") || g_str_equal(messagetype, "Text"))) {
			gchar *html;
			gint64 skypeemoteoffset = 0;
			PurpleChatUserFlags cbflags;
			PurpleChatUser *cb;
			
			if (json_object_has_member(resource, "skypeemoteoffset")) {
				skypeemoteoffset = g_ascii_strtoll(json_object_get_string_member(resource, "skypeemoteoffset"), NULL, 10);
			}
			
			from = skypeweb_contact_url_to_name(from);
			g_return_if_fail(from);
			
			// Remove typing notification icon w/o "show-typing-as-icon" option check.
			// Hard reset cbflags even if user changed settings while someone typing message.
			
			cb = purple_chat_conversation_find_user(chatconv, from);
			if (cb != NULL) {
				cbflags = purple_chat_user_get_flags(cb);
				
				cbflags &= ~PURPLE_CHAT_USER_TYPING & ~PURPLE_CHAT_USER_VOICE;
				
				purple_chat_user_set_flags(cb, cbflags);
			}
			
			if (content && *content) {
				if (g_str_equal(messagetype, "Text")) {
					gchar *temp = skypeweb_meify(content, skypeemoteoffset);
					html = purple_markup_escape_text(temp, -1);
					g_free(temp);
				} else {
					html = skypeweb_meify(content, skypeemoteoffset);
				}

				if (skypeeditedid && *skypeeditedid) {
					gchar *temp = g_strconcat(_("Edited: "), html, NULL);
					g_free(html);
					html = temp;
				}
				
				purple_serv_got_chat_in(sa->pc, g_str_hash(chatname), from, skypeweb_is_user_self(sa, from) ? PURPLE_MESSAGE_SEND : PURPLE_MESSAGE_RECV, html, composetimestamp);
						
				g_free(html);
			}
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
				if (skypeweb_is_user_self(sa, &user[2]))
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
					if (g_str_equal(role, "Admin") || g_str_equal(role, "admin")) {
						cbflags = PURPLE_CHAT_USER_OP;
					} else if (g_str_equal(role, "User") || g_str_equal(role, "user")) {
						//cbflags = PURPLE_CHAT_USER_VOICE;
					}
				}
				#if !PURPLE_VERSION_CHECK(3, 0, 0)
					purple_conv_chat_user_set_flags(chatconv, &user[2], cbflags);
					(void) cb;
				#else
					cb = purple_chat_conversation_find_user(chatconv, &user[2]);
					purple_chat_user_set_flags(cb, cbflags);
				#endif
				g_free(user);
				g_free(role);
			} 
			
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "RichText/UriObject")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			const gchar *uri = purple_xmlnode_get_attrib(blob, "url_thumbnail");
			
			skypeweb_download_uri_to_conv(sa, uri, conv, composetimestamp);
			purple_xmlnode_free(blob);
		} else {
			purple_debug_warning("skypeweb", "Unhandled thread message resource messagetype '%s'\n", messagetype);
		}
		
	} else {
		gchar *convbuddyname;
		// This is a One-to-one/IM message
		
		convbuddyname = g_strdup(skypeweb_contact_url_to_name(conversationLink));
		convname = g_strconcat(skypeweb_user_url_prefix(convbuddyname), convbuddyname, NULL);
		
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
			
			if (json_object_has_member(resource, "skypeemoteoffset")) {
				skypeemoteoffset = g_ascii_strtoll(json_object_get_string_member(resource, "skypeemoteoffset"), NULL, 10);
			}
			
			if (g_str_equal(messagetype, "Text")) {
				gchar *temp = skypeweb_meify(content, skypeemoteoffset);
				html = purple_markup_escape_text(temp, -1);
				g_free(temp);
			} else {
				html = skypeweb_meify(content, skypeemoteoffset);
			}

			if (skypeeditedid && *skypeeditedid) {
				gchar *temp = g_strconcat(_("Edited: "), html, NULL);
				g_free(html);
				html = temp;
			}
			
			if (json_object_has_member(resource, "imdisplayname")) {
				//TODO use this for an alias
			}
			
			if (skypeweb_is_user_self(sa, from)) {
				if (!g_str_has_prefix(html, "?OTR")) {
					PurpleMessage *msg;
					imconv = purple_conversations_find_im_with_account(convbuddyname, sa->account);
					if (imconv == NULL)
					{
						imconv = purple_im_conversation_new(sa->account, convbuddyname);
					}
					conv = PURPLE_CONVERSATION(imconv);
					
					msg = purple_message_new_outgoing(convbuddyname, html, PURPLE_MESSAGE_SEND);
					purple_message_set_time(msg, composetimestamp);
					purple_conversation_write_message(conv, msg);
					purple_message_destroy(msg);
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
			
			if (skypeweb_is_user_self(sa, from)) {
				from = convbuddyname;
			}
			if (from != NULL) {
				imconv = purple_conversations_find_im_with_account(from, sa->account);
				if (imconv == NULL)
				{
					imconv = purple_im_conversation_new(sa->account, from);
				}
				
				conv = PURPLE_CONVERSATION(imconv);
				skypeweb_download_uri_to_conv(sa, uri, conv, composetimestamp);
			}
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "RichText/Media_GenericFile")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			const gchar *uri = purple_xmlnode_get_attrib(blob, "uri");
			
			if (!skypeweb_is_user_self(sa, from)) {
				
				skypeweb_present_uri_as_filetransfer(sa, uri, from);
				
				from = convbuddyname;
			}
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "Event/SkypeVideoMessage")) {
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);
			const gchar *sid = purple_xmlnode_get_attrib(blob, "sid");
			PurpleIMConversation *imconv;
			
			if (skypeweb_is_user_self(sa, from)) {
				from = convbuddyname;
			}
			if (from != NULL) {
				imconv = purple_conversations_find_im_with_account(from, sa->account);
				if (imconv == NULL)
				{
					imconv = purple_im_conversation_new(sa->account, from);
				}
				
				conv = PURPLE_CONVERSATION(imconv);
				//skypeweb_download_video_message(sa, sid, conv); //TODO
				(void) sid;
				purple_serv_got_im(sa->pc, from, content, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, composetimestamp);
			}
			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "Event/Call")) {
			PurpleXmlNode *partlist = purple_xmlnode_from_str(content, -1);
			const gchar *partlisttype = purple_xmlnode_get_attrib(partlist, "type");
			const gchar *message = NULL;
			PurpleIMConversation *imconv;
			gboolean incoming = TRUE;
			
			if (skypeweb_is_user_self(sa, from)) {
				incoming = FALSE;
				(void) incoming;
				from = convbuddyname;
			}

			if (from != NULL) {
				if (partlisttype) {
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
							if (identity && skypeweb_is_user_self(sa, identity)) {
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
				}
				else {
					message = _("Unsupported call received");
				}

				purple_serv_got_im(sa->pc, from, message, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, composetimestamp);
			}

			purple_xmlnode_free(partlist);
		} else if (g_str_equal(messagetype, "Signal/Flamingo")) {
			const gchar *message = NULL;

			if (skypeweb_is_user_self(sa, from)) {
				from = convbuddyname;
			}

			if (from != NULL) {
				message = _("Unsupported call received");

				purple_serv_got_im(sa->pc, from, message, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, composetimestamp);
			}
		} else if (g_str_equal(messagetype, "RichText/Contacts")) {
			PurpleXmlNode *contacts = purple_xmlnode_from_str(content, -1);
			PurpleXmlNode *contact;

			for(contact = purple_xmlnode_get_child(contacts, "c"); contact;
				contact = purple_xmlnode_get_next_twin(contact))
			{
				const gchar *contact_id = purple_xmlnode_get_attrib(contact, "s");
				const gchar *contact_name = purple_xmlnode_get_attrib(contact, "f");

				gchar *message = g_strdup_printf(_("The user sent a contact: %s (%s)"), contact_id, contact_name); 

				purple_serv_got_im(sa->pc, from, message, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, composetimestamp);

				g_free(message);
			}

			skypeweb_received_contacts(sa, contacts);
			purple_xmlnode_free(contacts);
		} else if (g_str_equal(messagetype, "RichText/Media_FlikMsg")) {
			
			
			PurpleXmlNode *blob = purple_xmlnode_from_str(content, -1);

			const gchar *url_thumbnail = purple_xmlnode_get_attrib(blob, "url_thumbnail");
			gchar *text = purple_markup_strip_html(content); //purple_xmlnode_get_data_unescaped doesn't work properly in this situation
			
			PurpleIMConversation *imconv;
			
			if (skypeweb_is_user_self(sa, from)) {
				from = convbuddyname;
			}
			if (from != NULL) {
				imconv = purple_conversations_find_im_with_account(from, sa->account);
				if (imconv == NULL) {
					imconv = purple_im_conversation_new(sa->account, from);
				}
				
				conv = PURPLE_CONVERSATION(imconv);

				skypeweb_download_moji_to_conv(sa, text, url_thumbnail, conv, composetimestamp);

				const gchar *message = _("The user sent a Moji");

				purple_serv_got_im(sa->pc, from, message, PURPLE_MESSAGE_NO_LOG, composetimestamp);

				g_free(text);
			}

			purple_xmlnode_free(blob);
		} else if (g_str_equal(messagetype, "RichText/Files")) {
			purple_serv_got_im(sa->pc, convbuddyname, _("The user sent files in an unsupported way"), PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_ERROR, composetimestamp);
		} else {
			purple_debug_warning("skypeweb", "Unhandled message resource messagetype '%s'\n", messagetype);
		}
		
		g_free(convbuddyname);
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
	(void) id;

	JsonObject *threadProperties;
	
	if (json_object_has_member(resource, "threadProperties")) {
		threadProperties = json_object_get_object_member(resource, "threadProperties");
	}
	(void) threadProperties;
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
	

	if (((int)time(NULL)) > sa->vdms_expiry) {
		skypeweb_get_vdms_token(sa);
	}

	if (node == NULL && ((int)time(NULL)) > sa->registration_expiry) {
		skypeweb_get_registration_token(sa);
		return;
	} 

	
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
			} else if (errorCode == 450) {
				// "Subscription requested could not be found."
				// No more Womens Weekly? :O
			}
		}
		
		//TODO record id of highest recieved id to make sure we dont process the same id twice
	}
	
	if (!purple_connection_is_disconnecting(sa->pc)) {
		sa->poll_timeout = purple_timeout_add_seconds(1, skypeweb_timeout, sa);
	}
}

void
skypeweb_poll(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, "/v1/users/ME/endpoints/SELF/subscriptions/0/poll", NULL, skypeweb_poll_cb, NULL, TRUE);
}

void
skypeweb_mark_conv_seen(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	PurpleConnection *pc = purple_conversation_get_connection(conv);
	if (!PURPLE_CONNECTION_IS_CONNECTED(pc))
		return;
	
	if (g_strcmp0(purple_protocol_get_id(purple_connection_get_protocol(pc)), SKYPEWEB_PLUGIN_ID))
		return;
	
	if (type == PURPLE_CONVERSATION_UPDATE_UNSEEN) {
		gchar *last_skypeweb_id = purple_conversation_get_data(conv, "last_skypeweb_id");
		
		if (last_skypeweb_id && *last_skypeweb_id) {
			SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
			gchar *post, *url, *convname;
			
			if (PURPLE_IS_IM_CONVERSATION(conv)) {
				const gchar *buddyname = purple_conversation_get_name(conv);
				convname = g_strconcat(skypeweb_user_url_prefix(buddyname), buddyname, NULL);
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
			if (g_str_equal(role, "Admin") || g_str_equal(role, "admin")) {
				cbflags = PURPLE_CHAT_USER_OP;
			} else if (g_str_equal(role, "User") || g_str_equal(role, "user")) {
				//cbflags = PURPLE_CHAT_USER_VOICE;
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
	url = g_strdup_printf("/v1/users/ME/conversations/%s/messages?startTime=%d000&pageSize=30&view=msnp24Equivalent&targetType=Passport|Skype|Lync|Thread|PSTN|Agent", purple_url_encode(convname), since);
	
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
	url = g_strdup_printf("/v1/users/ME/conversations?startTime=%d000&pageSize=100&view=msnp24Equivalent&targetType=Passport|Skype|Lync|Thread|PSTN|Agent", since);
	
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
	
	url = g_strconcat(contacts_url, "/", skypeweb_user_url_prefix(who), purple_url_encode(who), NULL);
	
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
		
		if (SKYPEWEB_BUDDY_IS_BOT(cur->data)) {
			purple_protocol_got_user_status(sa->account, cur->data, SKYPEWEB_STATUS_ONLINE, NULL);
			continue;
		}
		
		id = g_strconcat(skypeweb_user_url_prefix(cur->data), cur->data, NULL);
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
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, "/v1/users/ME/endpoints/SELF/properties?name=supportsMessageProperties", "{\"supportsMessageProperties\":true}", NULL, NULL, TRUE);

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
skypeweb_got_registration_token(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	const gchar *registration_token = NULL;
	gchar *endpointId = NULL;
	gchar *expires = NULL;
	SkypeWebAccount *sa = user_data;
	gchar *new_messages_host = NULL;
	const gchar *data;
	gsize len;
	
	data = purple_http_response_get_data(response, &len);
	
	if (data == NULL) {
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
	
	new_messages_host = skypeweb_string_get_chunk(purple_http_response_get_header(response, "Location"), -1, "https://", "/");
	if (new_messages_host != NULL && !g_str_equal(sa->messages_host, new_messages_host)) {
		g_free(sa->messages_host);
		sa->messages_host = new_messages_host;
		
		// Your princess is in another castle
		purple_debug_info("skypeweb", "Messages host has changed to %s\n", sa->messages_host);
		
		skypeweb_get_registration_token(sa);
		return;
	}
	g_free(new_messages_host);
	
	registration_token = purple_http_response_get_header(response, "Set-RegistrationToken");
	
	if (registration_token == NULL) {
		if (purple_account_get_string(sa->account, "refresh-token", NULL)) {
			skypeweb_refresh_token_login(sa);
		} else {
			purple_connection_error (sa->pc,
									PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
									_("Failed getting Registration Token"));
		}
		return;
	}
	//purple_debug_info("skypeweb", "New RegistrationToken is %s\n", registration_token);
	endpointId = skypeweb_string_get_chunk(registration_token, -1, "endpointId=", NULL);
	expires = skypeweb_string_get_chunk(registration_token, -1, "expires=", ";");
	
	g_free(sa->registration_token); sa->registration_token = g_strndup(registration_token, strchr(registration_token, ';') - registration_token);
	g_free(sa->endpoint); sa->endpoint = endpointId;
	if (expires && *expires) {
		sa->registration_expiry = atoi(expires);
		g_free(expires);
	}
	
	if (sa->endpoint) {
		gchar *url = g_strdup_printf("/v1/users/ME/endpoints/%s/presenceDocs/messagingService", purple_url_encode(sa->endpoint));
		const gchar *post = "{\"id\":\"messagingService\", \"type\":\"EndpointPresenceDoc\", \"selfLink\":\"uri\", \"privateInfo\":{\"epname\":\"skype\"}, \"publicInfo\":{\"capabilities\":\"\", \"type\":1, \"typ\":1, \"skypeNameVersion\":\"" SKYPEWEB_CLIENTINFO_VERSION "/" SKYPEWEB_CLIENTINFO_NAME "\", \"nodeInfo\":\"\", \"version\":\"" SKYPEWEB_CLIENTINFO_VERSION "\"}}";
		skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, sa->messages_host, url, post, NULL, NULL, TRUE);
		g_free(url);
	}
	
	skypeweb_gather_self_properties(sa);	
	skypeweb_subscribe(sa);
}

static void
skypeweb_got_vdms_token(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	const gchar *token;
	SkypeWebAccount *sa = user_data;
	JsonParser *parser = json_parser_new();
	const gchar *data;
	gsize len;
	
	data = purple_http_response_get_data(response, &len);

	if (json_parser_load_from_data(parser, data, len, NULL)) {
		JsonNode *root = json_parser_get_root(parser);
		JsonObject *obj = json_node_get_object(root);

		token = json_object_get_string_member(obj, "token");
		sa->vdms_token = g_strdup(token); 
		sa->vdms_expiry = (int)time(NULL) + SKYPEWEB_VDMS_TTL;
	}

	g_object_unref(parser);
	
}

void
skypeweb_get_registration_token(SkypeWebAccount *sa)
{
	gchar *messages_url;
	PurpleHttpRequest *request;
	gchar *curtime;
	gchar *response;
	
	g_free(sa->registration_token); sa->registration_token = NULL;
	g_free(sa->endpoint); sa->endpoint = NULL;
	
	curtime = g_strdup_printf("%d", (int) time(NULL));
	response = skypeweb_hmac_sha256(curtime);
	
	messages_url = g_strdup_printf("https://%s/v1/users/ME/endpoints", sa->messages_host);
	
	request = purple_http_request_new(messages_url);
	purple_http_request_set_method(request, "POST");
	purple_http_request_set_keepalive_pool(request, sa->keepalive_pool);
	purple_http_request_set_max_redirects(request, 0);
	purple_http_request_header_set(request, "Accept", "*/*");
	purple_http_request_header_set(request, "BehaviorOverride", "redirectAs404");
	purple_http_request_header_set_printf(request, "LockAndKey", "appId=" SKYPEWEB_LOCKANDKEY_APPID "; time=%s; lockAndKeyResponse=%s", curtime, response);
	purple_http_request_header_set(request, "ClientInfo", "os=Windows; osVer=8.1; proc=Win32; lcid=en-us; deviceType=1; country=n/a; clientName=" SKYPEWEB_CLIENTINFO_NAME "; clientVer=" SKYPEWEB_CLIENTINFO_VERSION);
	purple_http_request_header_set(request, "Content-Type", "application/json");
	purple_http_request_header_set_printf(request, "Authentication", "skypetoken=%s", sa->skype_token);
	purple_http_request_set_contents(request, "{\"endpointFeatures\":\"Agent\"}", -1);
	purple_http_request(sa->pc, request, skypeweb_got_registration_token, sa);
	purple_http_request_unref(request);
	
	g_free(curtime);
	g_free(response);
	g_free(messages_url);
}

void
skypeweb_get_vdms_token(SkypeWebAccount *sa)
{
	const gchar *messages_url = "https://" SKYPEWEB_STATIC_HOST "/pes/v1/petoken";
	PurpleHttpRequest *request;
	
	request = purple_http_request_new(messages_url);
	purple_http_request_set_keepalive_pool(request, sa->keepalive_pool);
	purple_http_request_header_set(request, "Accept", "*/*");
	purple_http_request_header_set(request, "Origin", "https://web.skype.com");
	purple_http_request_header_set_printf(request, "Authorization", "skype_token %s", sa->skype_token);
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded");
	purple_http_request_set_contents(request, "{}", -1);
	purple_http_request(sa->pc, request, skypeweb_got_vdms_token, sa);
	purple_http_request_unref(request);
}


guint
skypeweb_conv_send_typing(PurpleConversation *conv, PurpleIMTypingState state)
{
	PurpleConnection *pc = purple_conversation_get_connection(conv);
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *post, *url;
	JsonObject *obj;
	
	if (!PURPLE_CONNECTION_IS_CONNECTED(pc))
		return 0;
	
	if (g_strcmp0(purple_protocol_get_id(purple_connection_get_protocol(pc)), SKYPEWEB_PLUGIN_ID))
		return 0;
	
	url = g_strdup_printf("/v1/users/ME/conversations/%s/messages", purple_url_encode(purple_conversation_get_name(conv)));
	
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

guint
skypeweb_send_typing(PurpleConnection *pc, const gchar *name, PurpleIMTypingState state)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *post, *url;
	JsonObject *obj;
	
	url = g_strdup_printf("/v1/users/ME/conversations/%s%s/messages", skypeweb_user_url_prefix(name), purple_url_encode(name));
	
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
}

void
skypeweb_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	
	skypeweb_set_statusid(sa, purple_status_get_id(status));
	skypeweb_set_mood_message(sa, purple_status_get_attr_string(status, "message"));
}

void
skypeweb_set_idle(PurpleConnection *pc, int time)
{
	const gchar *status_id;
	PurpleStatus *status;

	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	
	status = purple_account_get_active_status(purple_connection_get_account(pc));
	status_id = purple_status_get_id(status);

	/* Only go idle if active status is online  */
	if (!strcmp(status_id, SKYPEWEB_STATUS_ONLINE)) {
		if (time < 30) {
			skypeweb_set_statusid(sa, SKYPEWEB_STATUS_ONLINE);
		} else {
			skypeweb_set_statusid(sa, SKYPEWEB_STATUS_IDLE);
		}
	}
}


static void
skypeweb_sent_message_cb(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	gchar *convname = user_data;
	JsonObject *obj = NULL;
	
	if (node != NULL && json_node_get_node_type(node) == JSON_NODE_OBJECT)
		obj = json_node_get_object(node);
	
	if (obj != NULL) {
		if (json_object_has_member(obj, "errorCode")) {
			PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(convname, sa->account);
			if (chatconv == NULL) {
				purple_conversation_present_error(skypeweb_strip_user_prefix(convname), sa->account, json_object_get_string_member(obj, "message"));
			} else {
				PurpleMessage *msg = purple_message_new_system(json_object_get_string_member(obj, "message"), PURPLE_MESSAGE_ERROR);
				purple_conversation_write_message(PURPLE_CONVERSATION(chatconv), msg);
				purple_message_destroy(msg);
			}
		}
	}
	
	g_free(convname);
}

static void
skypeweb_send_message(SkypeWebAccount *sa, const gchar *convname, const gchar *message)
{
	gchar *post, *url;
	JsonObject *obj;
	gint64 clientmessageid;
	gchar *clientmessageid_str;
	gchar *stripped;
	static GRegex *font_strip_regex = NULL;
	gchar *font_stripped;
	
	url = g_strdup_printf("/v1/users/ME/conversations/%s/messages", purple_url_encode(convname));
	
	clientmessageid = skypeweb_get_js_time();
	clientmessageid_str = g_strdup_printf("%" G_GINT64_FORMAT "", clientmessageid);
	
	// Some clients don't receive messages with <br>'s in them
	stripped = purple_strreplace(message, "<br>", "\r\n");
	
	// Pidgin has a nasty habit of sending <font size="3"> when copy-pasting text
	if (font_strip_regex == NULL) {
		font_strip_regex = g_regex_new("(<font [^>]*)size=\"[0-9]+\"([^>]*>)", G_REGEX_CASELESS | G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
	}
	font_stripped = g_regex_replace(font_strip_regex, stripped, -1, 0, "\\1\\2", 0, NULL);
	if (font_stripped != NULL) {
		g_free(stripped);
		stripped = font_stripped;
	}
	
	obj = json_object_new();
	json_object_set_string_member(obj, "clientmessageid", clientmessageid_str);
	json_object_set_string_member(obj, "content", stripped);
	if (G_UNLIKELY(g_str_has_prefix(message, "<URIObject "))) {
		json_object_set_string_member(obj, "messagetype", "RichText/Media_GenericFile"); //hax!
	} else {
		json_object_set_string_member(obj, "messagetype", "RichText");
	}
	json_object_set_string_member(obj, "contenttype", "text");
	json_object_set_string_member(obj, "imdisplayname", sa->self_display_name ? sa->self_display_name : sa->username);
	
	if (g_str_has_prefix(message, "/me ")) {
		json_object_set_string_member(obj, "skypeemoteoffset", "4"); //Why is this a string :(
	}
	
	post = skypeweb_jsonobj_to_string(obj);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, sa->messages_host, url, post, skypeweb_sent_message_cb, g_strdup(convname), TRUE);
	
	g_free(post);
	json_object_unref(obj);
	g_free(url);
	g_free(stripped);
	
	g_hash_table_insert(sa->sent_messages_hash, clientmessageid_str, clientmessageid_str);
}


gint
skypeweb_chat_send(PurpleConnection *pc, gint id, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg)
{
	const gchar *message = purple_message_get_contents(msg);
#else
const gchar *message, PurpleMessageFlags flags)
{
#endif
	
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	
	PurpleChatConversation *chatconv;
	const gchar* chatname;
	
	chatconv = purple_conversations_find_chat(pc, id);
	chatname = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "chatname");
	if (!chatname) {
		// Fix for a condition around the chat data and serv_got_joined_chat()
		chatname = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
		if (!chatname)
			return -1;
        }

	skypeweb_send_message(sa, chatname, message);

	purple_serv_got_chat_in(pc, id, sa->username, PURPLE_MESSAGE_SEND, message, time(NULL));

	return 1;
}

gint
skypeweb_send_im(PurpleConnection *pc, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg)
{
	const gchar *who = purple_message_get_recipient(msg);
	const gchar *message = purple_message_get_contents(msg);
#else
const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
#endif

	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *convname;
	
	convname = g_strconcat(skypeweb_user_url_prefix(who), who, NULL);
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
	g_string_append_printf(url, "%s%s", skypeweb_user_url_prefix(who), purple_url_encode(who));
	
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
	g_string_append_printf(url, "%s%s", skypeweb_user_url_prefix(who), purple_url_encode(who));
	
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
	id = g_strconcat(skypeweb_user_url_prefix(who), who, NULL);
	json_object_set_string_member(contact, "id", id);
	json_object_set_string_member(contact, "role", "User");
	json_array_add_object_element(members, contact);
	g_free(id);
	
	contact = json_object_new();
	id = g_strconcat(skypeweb_user_url_prefix(sa->username), sa->username, NULL);
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

void
skypeweb_get_thread_url(SkypeWebAccount *sa, const gchar *thread)
{
	//POST https://api.scheduler.skype.com/threads
	//{"baseDomain":"https://join.skype.com/launch/","threadId":"%s"}
	
	// {"Id":"MeMxigEAAAAxOTo5NDZkMjExMGQ4YmU0ZjQzODc3NjMxNDQ3ZTgxYWNmNkB0aHJlYWQuc2t5cGU","Blob":null,"JoinUrl":"https://join.skype.com/ALXsHZ2RFQnk","ThreadId":"19:946d2110d8be4f43877631447e81acf6@thread.skype"}
}


static void
skypeweb_got_self_properties(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonObject *userobj;
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
	userobj = json_node_get_object(node);
	
	if (json_object_has_member(userobj, "primaryMemberName")) {
		g_free(sa->primary_member_name); sa->primary_member_name = g_strdup(json_object_get_string_member(userobj, "primaryMemberName"));
	}
}

void
skypeweb_gather_self_properties(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, sa->messages_host, "/v1/users/ME/properties", NULL, skypeweb_got_self_properties, NULL, TRUE);
}
