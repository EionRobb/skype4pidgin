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
 
#ifndef SKYPEWEB_CONTACTS_H
#define SKYPEWEB_CONTACTS_H

#include "libskypeweb.h"
#include "skypeweb_messages.h"

void skypeweb_get_icon(PurpleBuddy *buddy);
void skypeweb_download_uri_to_conv(SkypeWebAccount *sa, const gchar *uri, SkypeWebURIType uri_type, PurpleConversation *conv, time_t ts, const gchar* from);
void skypeweb_download_video_message(SkypeWebAccount *sa, const gchar *sid, PurpleConversation *conv);
void skypeweb_download_moji_to_conv(SkypeWebAccount *sa, const gchar *text, const gchar *url_thumbnail, PurpleConversation *conv, time_t ts, const gchar* from);
void skypeweb_present_uri_as_filetransfer(SkypeWebAccount *sa, const gchar *uri, const gchar *from);

PurpleXfer *skypeweb_new_xfer(
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleProtocolXfer *prplxfer, 
#endif
PurpleConnection *pc, const char *who);

void skypeweb_send_file(
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleProtocolXfer *prplxfer, 
#endif
PurpleConnection *pc, const gchar *who, const gchar *filename);

gboolean skypeweb_can_receive_file(
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleProtocolXfer *prplxfer, 
#endif
PurpleConnection *pc, const gchar *who);

void skypeweb_search_users(PurpleProtocolAction *action);

void skypeweb_received_contacts(SkypeWebAccount *sa, PurpleXmlNode *contacts);

void skypeweb_get_friend_profiles(SkypeWebAccount *sa, GSList *contacts);
void skypeweb_get_friend_profile(SkypeWebAccount *sa, const gchar *who);

void skypeweb_get_friend_list(SkypeWebAccount *sa);
void skypeweb_get_info(PurpleConnection *pc, const gchar *username);
void skypeweb_get_self_details(SkypeWebAccount *sa);

void skypeweb_buddy_remove(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group);
void skypeweb_add_buddy_with_invite(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char* message);
void skypeweb_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group);
void skypeweb_buddy_block(PurpleConnection *pc, const char *name);
void skypeweb_buddy_unblock(PurpleConnection *pc, const char *name);

gboolean skypeweb_check_authrequests(SkypeWebAccount *sa);

void skypeweb_set_mood_message(SkypeWebAccount *sa, const gchar *mood);

void skypeweb_contact_suggestions(PurpleProtocolAction *action);

#endif /* SKYPEWEB_CONTACTS_H */
