

#ifndef LIBSKYPEWEB_H
#define LIBSKYPEWEB_H

/* Maximum number of simultaneous connections to a server */
#define SKYPEWEB_MAX_CONNECTIONS 16

#include <glib.h>

#include <errno.h>
#include <string.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#ifdef __GNUC__
	#include <unistd.h>
#endif

#ifndef G_GNUC_NULL_TERMINATED
#	if __GNUC__ >= 4
#		define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#	else
#		define G_GNUC_NULL_TERMINATED
#	endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#ifdef _WIN32
#	include "win32dep.h"
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

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "accountopt.h"
#include "blist.h"
#include "core.h"
#include "connection.h"
#include "debug.h"
#include "dnsquery.h"
#include "proxy.h"
#include "prpl.h"
#include "request.h"
#include "savedstatuses.h"
#include "sslconn.h"
#include "util.h"
#include "version.h"

#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 12
#	define atoll(a) g_ascii_strtoll(a, NULL, 0)
#endif

#if !PURPLE_VERSION_CHECK(3, 0, 0)
	#define purple_connection_error purple_connection_error_reason
	#define purple_notify_user_info_add_pair_html purple_notify_user_info_add_pair
	#define purple_util_fetch_url_request purple_util_fetch_url_request_len_with_account
#endif

#define SKYPEWEB_MAX_MSG_RETRY 2

#define SKYPEWEB_PLUGIN_ID "prpl-skypeweb"
#define SKYPEWEB_PLUGIN_VERSION "0.1"

#define SKYPEWEB_LOCKANDKEY_APPID "msmsgs@msnmsgr.com"
#define SKYPEWEB_LOCKANDKEY_SECRET "Q1P7W2E4J9R8U3S5"

#define SKYPEWEB_CONTACTS_HOST "api.skype.com"
#define SKYPEWEB_MESSAGES_HOST "client-s.gateway.messenger.live.com"
#define SKYPEWEB_LOGIN_HOST "login.skype.com"
#define SKYPEWEB_VIDEOMAIL_HOST "vm.skype.com"

typedef struct _SkypeWebAccount SkypeWebAccount;
typedef struct _SkypeWebBuddy SkypeWebBuddy;

typedef void (*SkypeWebFunc)(SkypeWebAccount *swa);

struct _SkypeWebAccount {
	gchar *username;
	
	PurpleAccount *account;
	PurpleConnection *pc;
	GSList *conns; /**< A list of all active SkypeWebConnections */
	GQueue *waiting_conns; /**< A list of all SkypeWebConnections waiting to process */
	GSList *dns_queries;
	GHashTable *cookie_table;
	GHashTable *hostname_ip_cache;
	
	GHashTable *sent_messages_hash;
	guint poll_timeout;
	guint watchdog_timeout;
	
	guint authcheck_timeout;
	
	gchar *skype_token;
	gchar *registration_token;
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
	gchar *rich_mood;
};


void skypeweb_do_all_the_things(SkypeWebAccount *sa);

#endif /* LIBSKYPEWEB_H */
