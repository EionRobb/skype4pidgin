
#ifndef SKYPEWEB_MESSAGES_H
#define SKYPEWEB_MESSAGES_H

#include "libskypeweb.h"

gint skypeweb_send_im(PurpleConnection *pc, const gchar *who, const gchar *msg, PurpleMessageFlags flags);
gint skypeweb_chat_send(PurpleConnection *pc, gint id, const gchar *message, PurpleMessageFlags flags);
void skypeweb_set_idle(PurpleConnection *pc, int time);
void skypeweb_set_status(PurpleAccount *account, PurpleStatus *status);
guint skypeweb_send_typing(PurpleConnection *pc, const gchar *name, PurpleTypingState state);
void skypeweb_poll(SkypeWebAccount *sa);
void skypeweb_get_registration_token(SkypeWebAccount *sa);
void skypeweb_chat_invite(PurpleConnection *pc, int id, const char *message, const char *who);
void skypeweb_initiate_chat(SkypeWebAccount *sa, const gchar *who);
void skypeweb_initiate_chat_from_node(PurpleBlistNode *node, gpointer userdata);

void skypeweb_subscribe_to_contact_status(SkypeWebAccount *sa, GSList *contacts);
void skypeweb_get_conversation_history_since(SkypeWebAccount *sa, const gchar *convname, time_t since);
void skypeweb_get_conversation_history(SkypeWebAccount *sa, const gchar *convname);
void skypeweb_get_thread_users(SkypeWebAccount *sa, const gchar *convname);
void skypeweb_get_all_conversations_since(SkypeWebAccount *sa, time_t since);
void skype_web_get_offline_history(SkypeWebAccount *sa);

#endif /* SKYPEWEB_MESSAGES_H */
