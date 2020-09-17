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

#include "libskypeweb.h"

gchar *skypeweb_string_get_chunk(const gchar *haystack, gsize len, const gchar *start, const gchar *end);

gchar *skypeweb_jsonobj_to_string(JsonObject *jsonobj);

const gchar *skypeweb_contact_url_to_name(const gchar *url);
const gchar *skypeweb_thread_url_to_name(const gchar *url);

gchar *skypeweb_hmac_sha256(gchar *input);

gint64 skypeweb_get_js_time();

PurpleAccount *find_acct(const char *prpl, const char *acct_id);

const gchar *skypeweb_user_url_prefix(const gchar *who);
const gchar *skypeweb_strip_user_prefix(const gchar *who);

#define SKYPEWEB_GET_HTML_FIELD_NAME_ID(target, pc, haystack, len, name, id) \
	target = skypeweb_string_get_chunk(haystack, len, "name=\"" name "\" id=\"" id "\" value=\"", "\""); \
	if (!target) { \
		purple_connection_error(pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting " name " value, please try logging in via browser first")); \
		return; \
	}
