/*
 * SkypeWeb Plugin for libpurple/Pidgin
 * Copyright (c) 2014-2020 Eion Robb
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
 
#ifndef SKYPEWEB_MESSAGES_H
#define SKYPEWEB_MESSAGES_H

#include "libskypeweb.h"

typedef enum
{
	SKYPEWEB_URI_TYPE_IMAGE,
	SKYPEWEB_URI_TYPE_VIDEO,
	SKYPEWEB_URI_TYPE_UNKNOWN
} SkypeWebURIType;

gint skypeweb_send_im(PurpleConnection *pc, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
	PurpleMessage *msg
#else
	const gchar *who, const gchar *message, PurpleMessageFlags flags
#endif
);

gint skypeweb_chat_send(PurpleConnection *pc, gint id, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg
#else
const gchar *message, PurpleMessageFlags flags
#endif
);

void skypeweb_set_idle(PurpleConnection *pc, int time);
void skypeweb_set_status(PurpleAccount *account, PurpleStatus *status);
guint skypeweb_conv_send_typing(PurpleConversation *conv, PurpleIMTypingState state);
guint skypeweb_send_typing(PurpleConnection *pc, const gchar *name, PurpleIMTypingState state);
void skypeweb_poll(SkypeWebAccount *sa);
void skypeweb_get_registration_token(SkypeWebAccount *sa);
void skypeweb_get_vdms_token(SkypeWebAccount *sa);
void skypeweb_chat_kick(PurpleConnection *pc, int id, const char *who);
void skypeweb_chat_invite(PurpleConnection *pc, int id, const char *message, const char *who);
void skypeweb_initiate_chat(SkypeWebAccount *sa, const gchar *who);
void skypeweb_initiate_chat_from_node(PurpleBlistNode *node, gpointer userdata);
PurpleRoomlist *skypeweb_roomlist_get_list(PurpleConnection *pc);
void skypeweb_chat_set_topic(PurpleConnection *pc, int id, const char *topic);

void skypeweb_subscribe_to_contact_status(SkypeWebAccount *sa, GSList *contacts);
void skypeweb_unsubscribe_from_contact_status(SkypeWebAccount *sa, const gchar *who);
void skypeweb_get_conversation_history_since(SkypeWebAccount *sa, const gchar *convname, gint since);
void skypeweb_get_conversation_history(SkypeWebAccount *sa, const gchar *convname);
void skypeweb_get_thread_users(SkypeWebAccount *sa, const gchar *convname);
void skypeweb_get_all_conversations_since(SkypeWebAccount *sa, gint since);
void skype_web_get_offline_history(SkypeWebAccount *sa);
void skypeweb_mark_conv_seen(PurpleConversation *conv, PurpleConversationUpdateType type);

void skypeweb_gather_self_properties(SkypeWebAccount *sa);
gboolean skypeweb_is_user_self(SkypeWebAccount *sa, const gchar *username);

#endif /* SKYPEWEB_MESSAGES_H */
