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

#ifndef LIBSKYPEWEB_H
#define LIBSKYPEWEB_H

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

/* Maximum number of simultaneous connections to a server */
#define SKYPEWEB_MAX_CONNECTIONS 16

#include <glib.h>

#include <errno.h>
#include <string.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#ifdef __GNUC__
	#include <sys/time.h>
	#include <unistd.h>
#endif

#ifndef G_GNUC_NULL_TERMINATED
#	if __GNUC__ >= 4
#		define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#	else
#		define G_GNUC_NULL_TERMINATED
#	endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#include "purplecompat.h"

#ifdef _WIN32
#	define dlopen(a,b) LoadLibrary(a)
#	define RTLD_LAZY
#	define dlsym(a,b) GetProcAddress(a,b)
#	define dlclose(a) FreeLibrary(a)
#else
#	include <arpa/inet.h>
#	include <dlfcn.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#endif

#include <json-glib/json-glib.h>

#define json_object_get_int_member(JSON_OBJECT, MEMBER) \
	(JSON_OBJECT && json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_int_member(JSON_OBJECT, MEMBER) : 0)
#define json_object_get_string_member(JSON_OBJECT, MEMBER) \
	(JSON_OBJECT && json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_string_member(JSON_OBJECT, MEMBER) : NULL)
#define json_object_get_array_member(JSON_OBJECT, MEMBER) \
	(JSON_OBJECT && json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_array_member(JSON_OBJECT, MEMBER) : NULL)
#define json_object_get_object_member(JSON_OBJECT, MEMBER) \
	(JSON_OBJECT && json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_object_member(JSON_OBJECT, MEMBER) : NULL)
#define json_object_get_boolean_member(JSON_OBJECT, MEMBER) \
	(JSON_OBJECT && json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_boolean_member(JSON_OBJECT, MEMBER) : FALSE)
#define json_array_get_length(JSON_ARRAY) \
	(JSON_ARRAY ? json_array_get_length(JSON_ARRAY) : 0)
#define json_node_get_array(JSON_NODE) \
	(JSON_NODE && JSON_NODE_TYPE(JSON_NODE) == JSON_NODE_ARRAY ? json_node_get_array(JSON_NODE) : NULL)

#include "accountopt.h"
#include "core.h"
#include "cmds.h"
#include "connection.h"
#include "debug.h"
#include "http.h"
#include "plugins.h"
#include "proxy.h"
#include "request.h"
#include "roomlist.h"
#include "savedstatuses.h"
#include "sslconn.h"
#include "util.h"
#include "version.h"

	
#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 12
#	define atoll(a) g_ascii_strtoll(a, NULL, 0)
#endif


#define SKYPEWEB_MAX_MSG_RETRY 2

#define SKYPEWEB_PLUGIN_ID "prpl-skypeweb"
#define SKYPEWEB_PLUGIN_VERSION "1.5"

#define SKYPEWEB_LOCKANDKEY_APPID "msmsgs@msnmsgr.com"
#define SKYPEWEB_LOCKANDKEY_SECRET "Q1P7W2E4J9R8U3S5"

#define SKYPEWEB_CONTACTS_HOST "api.skype.com"
#define SKYPEWEB_NEW_CONTACTS_HOST "contacts.skype.com"
#define SKYPEWEB_DEFAULT_MESSAGES_HOST "client-s.gateway.messenger.live.com"
#define SKYPEWEB_LOGIN_HOST "login.skype.com"
#define SKYPEWEB_VIDEOMAIL_HOST "vm.skype.com"
#define SKYPEWEB_XFER_HOST "api.asm.skype.com"
#define SKYPEWEB_GRAPH_HOST "skypegraph.skype.com"
#define SKYPEWEB_STATIC_HOST "static.asm.skype.com"
#define SKYPEWEB_STATIC_CDN_HOST "static-asm.secure.skypeassets.com"
#define SKYPEWEB_DEFAULT_CONTACT_SUGGESTIONS_HOST "peoplerecommendations.skype.com"

#define SKYPEWEB_VDMS_TTL 300

#define SKYPEWEB_CLIENTINFO_NAME "swx-skype.com"
#define SKYPEWEB_CLIENTINFO_VERSION "908/1.85.0.29"

#define SKYPEWEB_STATUS_ONLINE "Online"
#define SKYPEWEB_STATUS_IDLE "Idle"
#define SKYPEWEB_STATUS_AWAY "Away"
#define SKYPEWEB_STATUS_BUSY "Busy"
#define SKYPEWEB_STATUS_HIDDEN "Hidden"
#define SKYPEWEB_STATUS_OFFLINE "Offline"


#define SKYPEWEB_BUDDY_IS_MSN(a) G_UNLIKELY((a) != NULL && strchr((a), '@') != NULL)
#define SKYPEWEB_BUDDY_IS_PHONE(a) G_UNLIKELY((a) != NULL && *(a) == '+')
#define SKYPEWEB_BUDDY_IS_S4B(a) G_UNLIKELY((a) != NULL && g_str_has_prefix((a), "2:"))
#define SKYPEWEB_BUDDY_IS_BOT(a) G_UNLIKELY((a) != NULL && g_str_has_prefix((a), "28:"))

typedef struct _SkypeWebAccount SkypeWebAccount;
typedef struct _SkypeWebBuddy SkypeWebBuddy;

typedef void (*SkypeWebFunc)(SkypeWebAccount *swa);

struct _SkypeWebAccount {
	gchar *username;
	gchar *primary_member_name;
	gchar *self_display_name;
	
	PurpleAccount *account;
	PurpleConnection *pc;
	PurpleHttpKeepalivePool *keepalive_pool;
	PurpleHttpConnectionSet *conns;
	PurpleHttpCookieJar *cookie_jar;
	gchar *messages_host;
	
	GHashTable *sent_messages_hash;
	guint poll_timeout;
	guint watchdog_timeout;
	
	guint authcheck_timeout;
	time_t last_authrequest;
	
	gchar *skype_token;
	gchar *registration_token;
	gchar *vdms_token;
	gchar *endpoint;
	gint registration_expiry;
	gint vdms_expiry;
};

struct _SkypeWebBuddy {
	SkypeWebAccount *sa;
	PurpleBuddy *buddy;
	
	/** Contact info */
	gchar *skypename;
	gchar *fullname;
	gchar *display_name;
	gboolean authorized;
	gboolean blocked;
	
	/** Profile info */
	gchar *avatar_url;
	gchar *mood;
};

void skypeweb_buddy_free(PurpleBuddy *buddy);

void skypeweb_do_all_the_things(SkypeWebAccount *sa);

#endif /* LIBSKYPEWEB_H */
