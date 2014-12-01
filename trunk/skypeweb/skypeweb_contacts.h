
#ifndef SKYPEWEB_CONTACTS_H
#define SKYPEWEB_CONTACTS_H

#include "libskypeweb.h"

void skypeweb_get_icon(PurpleBuddy *buddy);
void skypeweb_search_users(PurplePluginAction *action);

void skypeweb_get_friend_profiles(SkypeWebAccount *sa, GSList *contacts);
void skypeweb_get_friend_profile(SkypeWebAccount *sa, const gchar *who);

void skypeweb_get_friend_list(SkypeWebAccount *sa);
void skypeweb_get_info(PurpleConnection *pc, const gchar *username);
void skypeweb_get_self_details(SkypeWebAccount *sa);

void skypeweb_buddy_remove(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group);
void skypeweb_add_buddy_with_invite(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char* message);
void skypeweb_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group);

gboolean skypeweb_check_authrequests(SkypeWebAccount *sa);

#endif /* SKYPEWEB_CONTACTS_H */
