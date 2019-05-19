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
 
#ifndef SKYPEWEB_LOGIN_H
#define SKYPEWEB_LOGIN_H

#include "libskypeweb.h"
#include "skypeweb_connection.h"

#include <util.h>

void skypeweb_logout(SkypeWebAccount *sa);
void skypeweb_begin_web_login(SkypeWebAccount *sa);
void skypeweb_begin_oauth_login(SkypeWebAccount *sa);
void skypeweb_refresh_token_login(SkypeWebAccount *sa);
void skypeweb_begin_soapy_login(SkypeWebAccount *sa);

#endif /* SKYPEWEB_LOGIN_H */
