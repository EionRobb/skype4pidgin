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
#include "core.h"
#include "cmds.h"
#include "connection.h"
#include "debug.h"
#include "dnsquery.h"
#include "proxy.h"
#include "request.h"
#include "roomlist.h"
#include "savedstatuses.h"
#include "sslconn.h"
#include "util.h"
#include "version.h"

#if !PURPLE_VERSION_CHECK(3, 0, 0)
	#include "blist.h"
	#include "prpl.h"
#else
	#include "buddylist.h"
	#include "plugins.h"
	#include "http.h"
#endif
	
#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 12
#	define atoll(a) g_ascii_strtoll(a, NULL, 0)
#endif

#if !PURPLE_VERSION_CHECK(3, 0, 0)
	#define purple_connection_error purple_connection_error_reason
	#define purple_notify_user_info_add_pair_html purple_notify_user_info_add_pair
	#define purple_util_fetch_url_request purple_util_fetch_url_request_len_with_account
	
	#define PurpleConversationUpdateType PurpleConvUpdateType
	#define PurpleChatConversation PurpleConvChat
	#define PurpleIMConversation PurpleConvIm
	#define PurpleProtocolAction PurplePluginAction
	#define PURPLE_IS_BUDDY PURPLE_BLIST_NODE_IS_BUDDY
	#define PurpleProtocolChatEntry struct proto_chat_entry
	#define PURPLE_CONNECTION_CONNECTED PURPLE_CONNECTED
	#define PURPLE_CONNECTION_CONNECTING PURPLE_CONNECTING
	#define PURPLE_CONNECTION_FLAG_HTML PURPLE_CONNECTION_HTML
	#define PURPLE_CONNECTION_FLAG_NO_BGCOLOR PURPLE_CONNECTION_NO_BGCOLOR
	#define PURPLE_CONNECTION_FLAG_NO_FONTSIZE PURPLE_CONNECTION_NO_FONTSIZE
	#define purple_connection_get_protocol_data(pc) ((pc)->proto_data)
	#define purple_connection_set_protocol_data(pc, data) ((pc)->proto_data = (data))
	#define purple_connection_set_flags(pc, flags) (pc->flags = flags)
	#define purple_connection_get_flags(pc) (pc->flags)
	#define purple_conversation_present_error purple_conv_present_error
	#define purple_account_privacy_check purple_privacy_check
	#define purple_account_get_private_alias purple_account_get_alias
	#define purple_account_set_private_alias purple_account_set_alias
	#define purple_serv_got_im serv_got_im
	#define purple_serv_got_typing serv_got_typing
	#define purple_serv_got_alias serv_got_alias
	#define PurpleIMTypingState PurpleTypingState
	#define PURPLE_IM_NOT_TYPING PURPLE_NOT_TYPING
	#define PURPLE_IM_TYPING PURPLE_TYPING
	#define PURPLE_IM_TYPED PURPLE_TYPED
	#define purple_blist_find_buddy purple_find_buddy
	#define purple_blist_find_group purple_find_group
	#define PurpleChatUser PurpleConvChatBuddy
	#define PurpleChatUserFlags PurpleConvChatBuddyFlags
	#define PURPLE_CHAT_USER_NONE PURPLE_CBFLAGS_NONE
	#define PURPLE_CHAT_USER_OP PURPLE_CBFLAGS_OP
	#define PURPLE_CHAT_USER_FOUNDER PURPLE_CBFLAGS_FOUNDER
	#define PURPLE_CHAT_USER_TYPING PURPLE_CBFLAGS_TYPING
	#define PURPLE_CHAT_USER_AWAY PURPLE_CBFLAGS_AWAY
	#define PURPLE_CHAT_USER_HALFOP PURPLE_CBFLAGS_HALFOP
	#define PURPLE_CHAT_USER_VOICE PURPLE_CBFLAGS_VOICE
	#define PURPLE_CHAT_USER_TYPING PURPLE_CBFLAGS_TYPING
	#define purple_chat_user_get_flags(cb) purple_conv_chat_user_get_flags(
	#define purple_serv_got_joined_chat(pc, id, name) PURPLE_CONV_CHAT(serv_got_joined_chat(pc, id, name))
	#define purple_serv_got_chat_invite serv_got_chat_invite
	#define purple_serv_got_chat_in serv_got_chat_in
	#define purple_chat_conversation_get_topic purple_conv_chat_get_topic
	#define purple_chat_conversation_set_topic purple_conv_chat_set_topic
	#define purple_chat_conversation_find_user(chat, name) purple_conv_chat_cb_find(chat, name)
	#define purple_chat_conversation_add_user purple_conv_chat_add_user
	#define purple_chat_conversation_leave purple_conv_chat_left
	#define purple_chat_conversation_has_left purple_conv_chat_has_left
	#define purple_chat_conversation_add_user purple_conv_chat_add_user
	#define purple_chat_conversation_remove_user purple_conv_chat_remove_user
	#define purple_chat_conversation_clear_users purple_conv_chat_clear_users
	#define purple_chat_conversation_get_id purple_conv_chat_get_id
	#define purple_protocol_got_user_status purple_prpl_got_user_status
	#define purple_protocol_got_user_idle purple_prpl_got_user_idle
	#define PURPLE_CONVERSATION_UPDATE_TOPIC PURPLE_CONV_UPDATE_TOPIC
	#define PURPLE_CONVERSATION_UPDATE_UNSEEN PURPLE_CONV_UPDATE_UNSEEN
	#define purple_conversations_find_chat(pc, id) PURPLE_CONV_CHAT(purple_find_chat(pc, id))
	#define purple_conversations_find_chat_with_account(name, account) PURPLE_CONV_CHAT(purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, name, account))
	#define purple_conversations_find_im_with_account(name, account) PURPLE_CONV_IM(purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, name, account))
	#define purple_chat_conversation_new(account, from) PURPLE_CONV_CHAT(purple_conversation_new(PURPLE_CONV_TYPE_CHAT, account, from))
	#define purple_im_conversation_new(account, from) PURPLE_CONV_IM(purple_conversation_new(PURPLE_CONV_TYPE_IM, account, from))
	#define PURPLE_CONVERSATION(chatorim) (chatorim == NULL ? NULL : chatorim->conv)
	#define PURPLE_IM_CONVERSATION(conv) PURPLE_CONV_IM(conv)
	#define PURPLE_CHAT_CONVERSATION(conv) PURPLE_CONV_CHAT(conv)
	#define PURPLE_IS_IM_CONVERSATION(conv) (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
	#define PURPLE_IS_CHAT_CONVERSATION(conv) (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
	#define purple_conversation_get_connection purple_conversation_get_gc
	#define PurpleMessage PurpleConvMessage
	#define purple_conversation_write_message(conv, msg) purple_conversation_write(conv, msg->who, msg->what, msg->flags, msg->when)
	#define PurpleXmlNode xmlnode
	#define purple_xmlnode_from_str xmlnode_from_str
	#define purple_xmlnode_get_child xmlnode_get_child
	#define purple_xmlnode_get_next_twin xmlnode_get_next_twin
	#define purple_xmlnode_get_data xmlnode_get_data
	#define purple_xmlnode_get_attrib xmlnode_get_attrib
	#define purple_xmlnode_free xmlnode_free
	#define PurpleHash PurpleCipherContext
	#define purple_sha256_hash_new() purple_cipher_context_new(purple_ciphers_find_cipher("sha256"), NULL)
	#define purple_hash_append purple_cipher_context_append
	#define purple_hash_digest(hash, data, len) purple_cipher_context_digest(hash, len, data, NULL)
	#define purple_hash_destroy purple_cipher_context_destroy
	#define PURPLE_CMD_FLAG_PROTOCOL_ONLY PURPLE_CMD_FLAG_PRPL_ONLY
	#define PURPLE_CMD_P_PLUGIN PURPLE_CMD_P_PRPL
	
/*typedef struct {
	PurpleConvChatBuddy cb;
	PurpleConvChat *chat;
} PurpleChatUser;*/

typedef struct {
	gchar *host;
	gint port;
	gchar *path;
	gchar *user;
	gchar *passwd;
} PurpleHttpURL;

static inline PurpleHttpURL *purple_http_url_parse(const gchar *url) {
	PurpleHttpURL *ret = g_new0(PurpleHttpURL, 1);
	purple_url_parse(url, &(ret->host), &(ret->port), &(ret->path), &(ret->user), &(ret->passwd));
	return ret;
}
	#define purple_http_url_get_host(httpurl) (httpurl->host)
	#define purple_http_url_get_path(httpurl) (httpurl->path)
static inline void purple_http_url_free(PurpleHttpURL *phl) { g_free(phl->host); g_free(phl->path); g_free(phl->user); g_free(phl->passwd); g_free(phl);  }

#else
	#define purple_conversation_set_data(conv, key, value) g_object_set_data(G_OBJECT(conv), key, value)
	#define purple_conversation_get_data(conv, key) g_object_get_data(G_OBJECT(conv), key)
	#define purple_hash_destroy g_object_unref
	#define PURPLE_TYPE_STRING G_TYPE_STRING
	
	
typedef struct _SkypeWebProtocol
{
	PurpleProtocol parent;
} SkypeWebProtocol;

typedef struct _SkypeWebProtocolClass
{
	PurpleProtocolClass parent_class;
} SkypeWebProtocolClass;

G_MODULE_EXPORT GType skypeweb_protocol_get_type(void);
#define SKYPEWEB_TYPE_PROTOCOL             (skypeweb_protocol_get_type())
#define SKYPEWEB_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), SKYPEWEB_TYPE_PROTOCOL, SkypeWebProtocol))
#define SKYPEWEB_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), SKYPEWEB_TYPE_PROTOCOL, SkypeWebProtocolClass))
#define SKYPEWEB_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), SKYPEWEB_TYPE_PROTOCOL))
#define SKYPEWEB_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), SKYPEWEB_TYPE_PROTOCOL))
#define SKYPEWEB_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), SKYPEWEB_TYPE_PROTOCOL, SkypeWebProtocolClass))

#endif

#define SKYPEWEB_MAX_MSG_RETRY 2

#define SKYPEWEB_PLUGIN_ID "prpl-skypeweb"
#define SKYPEWEB_PLUGIN_VERSION "0.1"

#define SKYPEWEB_LOCKANDKEY_APPID "msmsgs@msnmsgr.com"
#define SKYPEWEB_LOCKANDKEY_SECRET "Q1P7W2E4J9R8U3S5"

#define SKYPEWEB_CONTACTS_HOST "api.skype.com"
#define SKYPEWEB_NEW_CONTACTS_HOST "contacts.skype.com"
#define SKYPEWEB_DEFAULT_MESSAGES_HOST "client-s.gateway.messenger.live.com"
#define SKYPEWEB_LOGIN_HOST "login.skype.com"
#define SKYPEWEB_VIDEOMAIL_HOST "vm.skype.com"

#define SKYPEWEB_CLIENTINFO_NAME "swx-skype.com"
#define SKYPEWEB_CLIENTINFO_VERSION "908/1.7.251"

#define SKYPEWEB_BUDDY_IS_MSN(a) G_UNLIKELY((a) != NULL && strchr((a), '@') != NULL)

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
	gchar *messages_host;
	
	GHashTable *sent_messages_hash;
	guint poll_timeout;
	guint watchdog_timeout;
	
	guint authcheck_timeout;
	time_t last_authrequest;
	
	gchar *skype_token;
	gchar *registration_token;
	gchar *endpoint;
	gint registration_expiry;

	GSList *url_datas; /**< PurpleUtilFetchUrlData to be cancelled on logout */
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
