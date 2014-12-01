
#ifndef SKYPEWEB_MESSAGES_H
#define SKYPEWEB_MESSAGES_H

#include "libskypeweb.h"

gint skypeweb_send_im(PurpleConnection *pc, const gchar *who, const gchar *msg, PurpleMessageFlags flags);
void skypeweb_set_idle(PurpleConnection *pc, int time);
void skypeweb_set_status(PurpleAccount *account, PurpleStatus *status);
guint skypeweb_send_typing(PurpleConnection *pc, const gchar *name, PurpleTypingState state);
void skypeweb_poll(SkypeWebAccount *sa);
void skypeweb_get_registration_token(SkypeWebAccount *sa);


void skypeweb_subscribe_to_contact_status(SkypeWebAccount *sa, GSList *contacts);

#endif /* SKYPEWEB_MESSAGES_H */
