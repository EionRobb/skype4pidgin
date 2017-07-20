#ifndef _PURPLECOMPAT_H_
#define _PURPLECOMPAT_H_

#include <glib.h>
#include "version.h"

#if PURPLE_VERSION_CHECK(3, 0, 0)
#include <glib-object.h>

#define purple_conversation_set_data(conv, key, value)  g_object_set_data(G_OBJECT(conv), key, value)
#define purple_conversation_get_data(conv, key)         g_object_get_data(G_OBJECT(conv), key)

#define purple_xfer_ref                 g_object_ref

#define purple_xfer_unref               g_object_unref
#define purple_circular_buffer_destroy  g_object_unref
#define purple_hash_destroy             g_object_unref
#define purple_message_destroy          g_object_unref
#define purple_buddy_destroy            g_object_unref

#define PURPLE_TYPE_STRING  G_TYPE_STRING

#define purple_protocol_action_get_connection(action)  ((action)->connection)

#define purple_chat_user_set_alias(cb, alias)  g_object_set((cb), "alias", (alias), NULL)
#define purple_chat_get_alias(chat)  g_object_get_data(G_OBJECT(chat), "alias")

//TODO remove this when dx adds this to the PurpleMessageFlags enum
#define PURPLE_MESSAGE_REMOTE_SEND  0x10000

#else /*!PURPLE_VERSION_CHECK(3, 0, 0)*/

#include "connection.h"

#define purple_blist_find_buddy        purple_find_buddy
#define purple_blist_find_group        purple_find_group
#define purple_blist_find_buddies      purple_find_buddies
#define PURPLE_IS_BUDDY                PURPLE_BLIST_NODE_IS_BUDDY
#define PURPLE_IS_CHAT                 PURPLE_BLIST_NODE_IS_CHAT
#define purple_chat_get_name_only      purple_chat_get_name
#define purple_chat_set_alias          purple_blist_alias_chat
#define purple_chat_get_alias(chat)    ((chat)->alias)
#define purple_buddy_set_server_alias  purple_blist_server_alias_buddy
#define purple_buddy_get_local_alias   purple_buddy_get_local_buddy_alias
static inline void
purple_blist_node_set_transient(PurpleBlistNode *node, gboolean transient)
{
	PurpleBlistNodeFlags old_flags = purple_blist_node_get_flags(node);
	PurpleBlistNodeFlags new_flags;
	
	if (transient)
		new_flags = old_flags | PURPLE_BLIST_NODE_FLAG_NO_SAVE;
	else
		new_flags = old_flags & ~PURPLE_BLIST_NODE_FLAG_NO_SAVE;
	
	purple_blist_node_set_flags(node, new_flags);
}

#define PURPLE_CMD_FLAG_PROTOCOL_ONLY  PURPLE_CMD_FLAG_PRPL_ONLY

#define PURPLE_TYPE_CONNECTION	purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION)
#define PURPLE_IS_CONNECTION	PURPLE_CONNECTION_IS_VALID

#define PURPLE_CONNECTION_DISCONNECTED     PURPLE_DISCONNECTED
#define PURPLE_CONNECTION_DISCONNECTING    4
#define PURPLE_CONNECTION_CONNECTING       PURPLE_CONNECTING
#define PURPLE_CONNECTION_CONNECTED        PURPLE_CONNECTED
#define PURPLE_CONNECTION_FLAG_HTML        PURPLE_CONNECTION_HTML
#define PURPLE_CONNECTION_FLAG_NO_BGCOLOR  PURPLE_CONNECTION_NO_BGCOLOR
#define PURPLE_CONNECTION_FLAG_NO_FONTSIZE PURPLE_CONNECTION_NO_FONTSIZE
#define PURPLE_CONNECTION_FLAG_NO_IMAGES   PURPLE_CONNECTION_NO_IMAGES

#define purple_request_cpar_from_connection(a)  purple_connection_get_account(a), NULL, NULL
#define purple_connection_get_protocol          purple_connection_get_prpl
#define purple_connection_error                 purple_connection_error_reason
#define purple_connection_is_disconnecting(c)   (purple_connection_get_state(c) == PURPLE_DISCONNECTED || purple_connection_get_state(c) == PURPLE_CONNECTION_DISCONNECTING)
#define purple_connection_set_flags(pc, f)      ((pc)->flags = (f))
#define purple_connection_get_flags(pc)         ((pc)->flags)

#define PurpleConversationUpdateType       PurpleConvUpdateType
#define PURPLE_CONVERSATION_UPDATE_TOPIC   PURPLE_CONV_UPDATE_TOPIC
#define PURPLE_CONVERSATION_UPDATE_UNSEEN  PURPLE_CONV_UPDATE_UNSEEN
#define PurpleChatConversation             PurpleConvChat
#define PurpleIMConversation               PurpleConvIm
#define purple_conversations_find_chat_with_account(id, account) \
		PURPLE_CONV_CHAT(purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, id, account))
#define purple_conversations_find_chat(pc, id)  PURPLE_CONV_CHAT(purple_find_chat(pc, id))
#define purple_conversations_get_all            purple_get_conversations
#define purple_conversation_get_connection      purple_conversation_get_gc
#define purple_conversation_present_error       purple_conv_present_error
#define purple_chat_conversation_get_id         purple_conv_chat_get_id
#define purple_chat_conversation_get_topic      purple_conv_chat_get_topic
#define purple_chat_conversation_set_topic      purple_conv_chat_set_topic
#define purple_conversations_find_im_with_account(name, account)  \
		PURPLE_CONV_IM(purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, name, account))
#define purple_im_conversation_new(account, from) PURPLE_CONV_IM(purple_conversation_new(PURPLE_CONV_TYPE_IM, account, from))
#define PURPLE_CONVERSATION(chatorim)         (chatorim == NULL ? NULL : chatorim->conv)
#define PURPLE_IM_CONVERSATION(conv)          PURPLE_CONV_IM(conv)
#define PURPLE_CHAT_CONVERSATION(conv)        PURPLE_CONV_CHAT(conv)
#define PURPLE_IS_IM_CONVERSATION(conv)       (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
#define PURPLE_IS_CHAT_CONVERSATION(conv)     (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
#define purple_chat_conversation_add_user     purple_conv_chat_add_user
#define purple_chat_conversation_has_left     purple_conv_chat_has_left
#define purple_chat_conversation_leave        purple_conv_chat_left
#define purple_chat_conversation_remove_user  purple_conv_chat_remove_user
#define purple_chat_conversation_clear_users  purple_conv_chat_clear_users

#define PurpleMessage  PurpleConvMessage
#define purple_message_get_contents(msg)    ((msg)->what)
#define purple_message_set_time(msg, time)  ((msg)->when = (time))
#define purple_conversation_write_message(conv, msg)  purple_conversation_write((conv), (msg)->who, (msg)->what, (msg)->flags, (msg)->when)
static inline PurpleMessage *
purple_message_new_outgoing(const gchar *who, const gchar *contents, PurpleMessageFlags flags)
{
	PurpleMessage *message = g_new0(PurpleMessage, 1);
	
	message->who = g_strdup(who);
	message->what = g_strdup(contents);
	message->flags = flags | PURPLE_MESSAGE_SEND;
	message->when = time(NULL);
	
	return message;
}
static inline PurpleMessage *
purple_message_new_system(const gchar *contents, PurpleMessageFlags flags)
{
	PurpleMessage *message = g_new0(PurpleMessage, 1);
	
	message->what = g_strdup(contents);
	message->flags = flags | PURPLE_MESSAGE_SYSTEM;
	message->when = time(NULL);
	
	return message;
}
static inline void
purple_message_destroy(PurpleMessage *message)
{
	g_free(message->who);
	g_free(message->what);
	g_free(message);
}
#if	!PURPLE_VERSION_CHECK(2, 12, 0)
#	define PURPLE_MESSAGE_REMOTE_SEND  0x10000
#endif

#define purple_conversation_write_system_message(conv, msg, flags)  purple_conversation_write((conv), NULL, (msg), (flags) | PURPLE_MESSAGE_SYSTEM, time(NULL));

#define PurpleProtocolChatEntry  struct proto_chat_entry
#define PurpleChatUserFlags  PurpleConvChatBuddyFlags
#define PURPLE_CHAT_USER_NONE     PURPLE_CBFLAGS_NONE
#define PURPLE_CHAT_USER_OP       PURPLE_CBFLAGS_OP
#define PURPLE_CHAT_USER_FOUNDER  PURPLE_CBFLAGS_FOUNDER
#define PURPLE_CHAT_USER_TYPING   PURPLE_CBFLAGS_TYPING
#define PURPLE_CHAT_USER_AWAY     PURPLE_CBFLAGS_AWAY
#define PURPLE_CHAT_USER_HALFOP   PURPLE_CBFLAGS_HALFOP
#define PURPLE_CHAT_USER_VOICE    PURPLE_CBFLAGS_VOICE
#define PURPLE_CHAT_USER_TYPING   PURPLE_CBFLAGS_TYPING
#define PurpleChatUser  PurpleConvChatBuddy

static inline PurpleChatUser *
purple_chat_conversation_find_user(PurpleChatConversation *chat, const char *name)
{
	PurpleChatUser *cb = purple_conv_chat_cb_find(chat, name);
	
	if (cb != NULL) {
		g_dataset_set_data(cb, "chat", chat);
	}
	
	return cb;
}
#define purple_chat_user_get_flags(cb)     purple_conv_chat_user_get_flags(g_dataset_get_data((cb), "chat"), (cb)->name)
#define purple_chat_user_set_flags(cb, f)  purple_conv_chat_user_set_flags(g_dataset_get_data((cb), "chat"), (cb)->name, (f))
#define purple_chat_user_set_alias(cb, a)  ((cb)->alias = (a))

#define PurpleIMTypingState	PurpleTypingState
#define PURPLE_IM_NOT_TYPING	PURPLE_NOT_TYPING
#define PURPLE_IM_TYPING	PURPLE_TYPING
#define PURPLE_IM_TYPED		PURPLE_TYPED

#define purple_media_set_protocol_data  purple_media_set_prpl_data
#if	PURPLE_VERSION_CHECK(2, 10, 12)
// Handle ABI breakage
#	define PURPLE_MEDIA_NETWORK_PROTOCOL_TCP  PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_PASSIVE
#endif

#undef purple_notify_error
#define purple_notify_error(handle, title, primary, secondary, cpar)   \
	purple_notify_message((handle), PURPLE_NOTIFY_MSG_ERROR, (title), \
						(primary), (secondary), NULL, NULL)
#undef purple_notify_warning
#define purple_notify_warning(handle, title, primary, secondary, cpar)   \
	purple_notify_message((handle), PURPLE_NOTIFY_MSG_WARNING, (title), \
						(primary), (secondary), NULL, NULL)
#define purple_notify_user_info_add_pair_html  purple_notify_user_info_add_pair

#define PurpleProtocolAction                           PurplePluginAction
#define purple_protocol_action_get_connection(action)  ((PurpleConnection *) (action)->context)
#define purple_protocol_action_new                     purple_plugin_action_new
#define purple_protocol_get_id                         purple_plugin_get_id

#define purple_protocol_got_user_status		purple_prpl_got_user_status
#define purple_protocol_got_user_idle       purple_prpl_got_user_idle

#define purple_account_privacy_deny_add     purple_privacy_deny_add
#define purple_account_privacy_deny_remove  purple_privacy_deny_remove
#define purple_account_set_password(account, password, dummy1, dummy2) \
		purple_account_set_password(account, password);
#define purple_account_set_private_alias    purple_account_set_alias
#define purple_account_get_private_alias    purple_account_get_alias

#define purple_proxy_info_get_proxy_type        purple_proxy_info_get_type

#define purple_serv_got_im                         serv_got_im
#define purple_serv_got_typing                     serv_got_typing
#define purple_serv_got_alias                      serv_got_alias
#define purple_serv_got_chat_in                    serv_got_chat_in
#define purple_serv_got_chat_left                  serv_got_chat_left
#define purple_serv_got_joined_chat(pc, id, name)  PURPLE_CONV_CHAT(serv_got_joined_chat(pc, id, name))

#define purple_status_get_status_type  purple_status_get_type

#define PurpleXmlNode                xmlnode
#define purple_xmlnode_new           xmlnode_new
#define purple_xmlnode_new_child     xmlnode_new_child
#define purple_xmlnode_from_str      xmlnode_from_str
#define purple_xmlnode_to_str        xmlnode_to_str
#define purple_xmlnode_get_child     xmlnode_get_child
#define purple_xmlnode_get_next_twin xmlnode_get_next_twin
#define purple_xmlnode_get_data      xmlnode_get_data
#define purple_xmlnode_get_attrib    xmlnode_get_attrib
#define purple_xmlnode_set_attrib    xmlnode_set_attrib
#define purple_xmlnode_insert_data   xmlnode_insert_data
#define purple_xmlnode_free          xmlnode_free

#define purple_xfer_set_protocol_data(xfer, proto_data)  ((xfer)->data = (proto_data))
#define purple_xfer_get_protocol_data(xfer)              ((xfer)->data)
#define purple_xfer_get_xfer_type                        purple_xfer_get_type
#define purple_xfer_protocol_ready                       purple_xfer_prpl_ready
#if !PURPLE_VERSION_CHECK(2, 10, 12) && !FEDORA
static inline gboolean
purple_xfer_write_file(PurpleXfer *xfer, const guchar *buffer, gsize size) {
	PurpleXferUiOps *ui_ops = purple_xfer_get_ui_ops(xfer);
	purple_xfer_set_bytes_sent(xfer, purple_xfer_get_bytes_sent(xfer) + 
		(ui_ops && ui_ops->ui_write ? ui_ops->ui_write(xfer, buffer, size) : fwrite(buffer, 1, size, xfer->dest_fp)));
	return TRUE;
}
#endif
#define PURPLE_XFER_TYPE_RECEIVE  PURPLE_XFER_RECEIVE
#define PURPLE_XFER_TYPE_SEND     PURPLE_XFER_SEND

#endif

#endif /*_PURPLECOMPAT_H_*/
