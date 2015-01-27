
#ifndef SKYPEWEB_LOGIN_H
#define SKYPEWEB_LOGIN_H

#include "libskypeweb.h"
#include "skypeweb_connection.h"

#include <util.h>

void skypeweb_logout(SkypeWebAccount *sa);
void skypeweb_begin_web_login(SkypeWebAccount *sa);
void skypeweb_begin_oauth_login(SkypeWebAccount *sa);

#endif /* SKYPEWEB_LOGIN_H */
