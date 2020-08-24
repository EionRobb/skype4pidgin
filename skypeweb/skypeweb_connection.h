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
 
#ifndef SKYPEWEB_CONNECTION_H
#define SKYPEWEB_CONNECTION_H

#include "libskypeweb.h"

typedef void (*SkypeWebProxyCallbackFunc)(SkypeWebAccount *sa, JsonNode *node, gpointer user_data);
typedef void (*SkypeWebProxyCallbackErrorFunc)(SkypeWebAccount *sa, const gchar *data, gssize data_len, gpointer user_data);

/*
 * This is a bitmask.
 */
typedef enum
{
	SKYPEWEB_METHOD_GET    = 0x0001,
	SKYPEWEB_METHOD_POST   = 0x0002,
	SKYPEWEB_METHOD_PUT    = 0x0004,
	SKYPEWEB_METHOD_DELETE = 0x0008,
	SKYPEWEB_METHOD_SSL    = 0x1000,
} SkypeWebMethod;

typedef struct _SkypeWebConnection SkypeWebConnection;
struct _SkypeWebConnection {
	SkypeWebAccount *sa;
	gchar *url;
	SkypeWebProxyCallbackFunc callback;
	gpointer user_data;
	PurpleHttpConnection *http_conn;
	SkypeWebProxyCallbackErrorFunc error_callback;
};

SkypeWebConnection *skypeweb_post_or_get(SkypeWebAccount *sa, SkypeWebMethod method,
		const gchar *host, const gchar *url, const gchar *postdata,
		SkypeWebProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive);

void skypeweb_update_cookies(SkypeWebAccount *sa, const gchar *headers);		
gchar *skypeweb_cookies_to_string(SkypeWebAccount *sa);

//SkypeWebConnection *skypeweb_fetch_url_request();

#endif /* SKYPEWEB_CONNECTION_H */
