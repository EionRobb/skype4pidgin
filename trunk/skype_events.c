/*
 * Skype plugin for libpurple/Pidgin/Adium
 * Written by: Eion Robb <eionrobb@gmail.com>
 *
 * This plugin uses the Skype API to show your contacts in libpurple, and send/receive
 * chat messages.
 * It requires the Skype program to be running.
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

#include <ft.h>

void skype_auth_allow(gpointer sender);
void skype_auth_deny(gpointer sender);
static gboolean skype_handle_received_message(char *message);
gint skype_find_filetransfer(PurpleXfer *transfer, char *skypeid);
void skype_accept_transfer(PurpleXfer *transfer);
void skype_decline_transfer(PurpleXfer *transfer);
SkypeChat *skype_find_chat(const gchar *chat_id, PurpleAccount *this_account);
gint skype_find_chat_compare_func(PurpleConversation *conv, char *chat_id);
static void purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatusType status);
void skype_call_accept_cb(gchar *call);
void skype_call_reject_cb(gchar *call);
void skype_call_ignore_cb(gchar *call);
void skype_call_voicemail_cb(gchar *call);
void skype_call_forward_cb(gchar *call);
gboolean skype_sync_skype_close(PurpleConnection *gc);
gboolean handle_complete_message(int messagenumber);

gboolean skype_update_buddy_status(PurpleBuddy *buddy);
void skype_update_buddy_alias(PurpleBuddy *buddy);
void skype_update_buddy_icon(PurpleBuddy *buddy);
static PurpleAccount *skype_get_account(PurpleAccount *account);
const char *skype_get_account_username(PurpleAccount *acct);
gchar *skype_get_user_info(const gchar *username, const gchar *property);
gchar *skype_strdup_withhtml(const gchar *src);
void skype_put_buddies_in_groups();
void skype_get_chatmessage_info(int message);
void set_skype_buddy_attribute(SkypeBuddy *sbuddy, const gchar *skype_buddy_property, const gchar *value);
SkypeBuddy *skype_buddy_new(PurpleBuddy *buddy);

char *skype_send_message(char *message, ...);
//dont use this unless you know what you're doing:
void skype_send_message_nowait(char *message, ...);

static time_t last_pong = 0;
static GHashTable *messages_table = NULL;
static GHashTable *groups_table = NULL;

typedef enum _SkypeMessageType {
	SKYPE_MESSAGE_UNSET = 0,
	SKYPE_MESSAGE_OTHER,
	SKYPE_MESSAGE_TEXT,
	SKYPE_MESSAGE_EMOTE,
	SKYPE_MESSAGE_ADD,
	SKYPE_MESSAGE_LEFT,
	SKYPE_MESSAGE_KICKED,
	SKYPE_MESSAGE_TOPIC
} SkypeMessageType;

typedef struct _SkypeMessage {
	//Required properties
	PurpleAccount *account;
	SkypeMessageType type;
	PurpleMessageFlags flags;  //send or recv
	gchar *chatname;
	
	//Optional properties
	gchar *body;		//topic, text, emote
	gchar *from_handle;	//topic, text, emote, left
	gint  timestamp;		//text, emote
	gchar **users;		//add, kicked
	gchar *leavereason;	//left
} SkypeMessage;

/*
	This function must only be called from the main loop, using purple_timeout_add
*/
static gboolean
skype_handle_received_message(char *message)
{
	char command[255];
	char **string_parts = NULL;
	PurpleAccount *this_account;
	PurpleConnection *gc;
	const char *my_username;
	PurpleBuddy *buddy;
	SkypeBuddy *sbuddy;
	SkypeChat *chat;
	char *body;
	char *body_html;
	char *msg_num;
	char *sender;
	char *type;
	int mtime;
	char *chatname;
	char *temp;
	//char *chat_type;
	char **chatusers = NULL;
	PurpleXfer *transfer = NULL;
	PurpleConversation *conv = NULL;
	GList *glist_temp = NULL;
	int i;
	PurpleGroup *temp_group;
	//PurpleStatusPrimitive primitive;
	SkypeMessage *skypemessage;

	sscanf(message, "%s ", command);
	this_account = skype_get_account(NULL);
	if (this_account == NULL)
		return FALSE;
	gc = purple_account_get_connection(this_account);
	my_username = skype_get_account_username(this_account);
	string_parts = g_strsplit(message, " ", 4);
	
	if (g_str_equal(command, "PONG"))
	{
		last_pong = time(NULL);
	} else if (g_str_equal(command, "USERSTATUS"))
	{

	} else if (g_str_equal(command, "CONNSTATUS"))
	{
		if (g_str_equal(string_parts[1], "LOGGEDOUT"))
		{
			//need to make this synchronous :(
			if (gc != NULL)
				purple_connection_error(gc, _("\nSkype program closed"));
			//purple_timeout_add(0, (GSourceFunc)skype_sync_skype_close, gc);
		}
	} else if (g_str_equal(command, "USER"))
	{
		buddy = purple_find_buddy(this_account, string_parts[1]);
		if (buddy != NULL)
		{
			sbuddy = buddy->proto_data;
			if (g_str_equal(string_parts[2], "ONLINESTATUS"))
			{
				//the status 'id' is in string_parts[3]
				
				//special cases:
				if (g_str_equal(string_parts[3], "SKYPEOUT"))
				{
					set_skype_buddy_attribute(sbuddy, "MOOD_TEXT", _("SkypeOut"));
				}
				if (g_str_equal(string_parts[3], "UNKNOWN"))
				{
					//user doesn't exist
					purple_blist_remove_buddy(buddy);
					purple_notify_error(gc, "Error", "User does not exist", "The user does not exist in Skype");
					buddy = NULL;
				} else {
					PurpleStatus *status = purple_presence_get_active_status(purple_buddy_get_presence(buddy));
					//Dont say we got their status unless its changed
					if (!status || !g_str_equal(purple_status_get_id(status), string_parts[3]))
					{
						purple_prpl_got_user_status(this_account, string_parts[1], string_parts[3], NULL);
					}
					//dont update buddy icon/mood for offline/skypeout users
					if (!g_str_equal(string_parts[3], "OFFLINE") && !g_str_equal(string_parts[3], "SKYPEOUT"))
					{
						skype_send_message_nowait("GET USER %s MOOD_TEXT", string_parts[1]);
						skype_update_buddy_icon(buddy);
					}
				}
			} else if (g_str_equal(string_parts[2], "DISPLAYNAME"))
			{
				if (strlen(g_strstrip(string_parts[3])))
					purple_blist_server_alias_buddy(buddy, string_parts[3]);
			} else if (g_str_equal(string_parts[2], "FULLNAME"))
			{
				if (strlen(g_strstrip(string_parts[3])) && (!purple_buddy_get_server_alias(buddy) || !strlen(purple_buddy_get_server_alias(buddy))))
					purple_blist_server_alias_buddy(buddy, string_parts[3]);
				set_skype_buddy_attribute(sbuddy, "FULLNAME", string_parts[3]);
			} else if ((g_str_equal(string_parts[2], "BUDDYSTATUS")) &&
					(g_str_equal(string_parts[3], "1")))
			{
				purple_blist_remove_buddy(buddy);
			} else if (g_str_equal(string_parts[2], "MOOD_TEXT"))
			{
				if (sbuddy && (!sbuddy->mood || !g_str_equal(sbuddy->mood, string_parts[3])))
				{
					set_skype_buddy_attribute(sbuddy, string_parts[2], string_parts[3]);
				}
			} else {
				set_skype_buddy_attribute(sbuddy, string_parts[2], string_parts[3]);
			}
		} else if (g_str_equal(string_parts[2], "BUDDYSTATUS"))
		{
			if (g_str_equal(string_parts[3], "3"))
			{
				skype_debug_info("skype", "Buddy %s just got added\n", string_parts[1]);
				//buddy just got added.. handle it
				if (purple_find_buddy(this_account, string_parts[1]) == NULL)
				{
					skype_debug_info("skype", "Buddy not in list\n");
					buddy = purple_buddy_new(this_account, g_strdup(string_parts[1]), NULL);
					skype_buddy_new(buddy);
					if (string_parts[1][0] == '+')
					{
						temp_group = purple_find_group("SkypeOut");
						if (temp_group == NULL)
						{
							temp_group = purple_group_new("SkypeOut");
							purple_blist_add_group(temp_group, NULL);
						}
					} else {
						temp_group = purple_find_group("Skype");
						if (temp_group == NULL)
						{
							temp_group = purple_group_new("Skype");
							purple_blist_add_group(temp_group, NULL);
						}
					}
					purple_blist_add_buddy(buddy, NULL, temp_group, NULL);
					skype_update_buddy_status(buddy);
					skype_update_buddy_alias(buddy);
					purple_prpl_got_user_idle(this_account, buddy->name, FALSE, 0);
					skype_update_buddy_icon(buddy);
					skype_put_buddies_in_groups();
				}
			}
		} else if (g_str_equal(string_parts[2], "RECEIVEDAUTHREQUEST"))
		{
			if (purple_account_get_bool(this_account, "reject_all_auths", FALSE))
			{
				skype_auth_deny(g_strdup(string_parts[1]));
			} else {
				//this event can be fired directly after authorising someone
				temp = skype_get_user_info(string_parts[1], "ISAUTHORIZED");
				if (!g_str_equal(temp, "TRUE") && string_parts[3])
				{
					skype_debug_info("skype", "User %s requested authorisation\n", string_parts[1]);
					purple_account_request_authorization(this_account, string_parts[1], NULL, skype_get_user_info(string_parts[1], "FULLNAME"),
														string_parts[3], (purple_find_buddy(this_account, string_parts[1]) != NULL),
														skype_auth_allow, skype_auth_deny, (gpointer)g_strdup(string_parts[1]));
				}
				g_free(temp);
			}
		}
	} else if (g_str_equal(command, "MESSAGE"))
	{
		if (g_str_equal(string_parts[3], "RECEIVED"))
		{
			msg_num = string_parts[1];
			temp = skype_send_message("GET MESSAGE %s TYPE", msg_num);
			type = g_strdup(&temp[14+strlen(msg_num)]);
			g_free(temp);
			if (g_str_equal(type, "TEXT") ||
				g_str_equal(type, "AUTHREQUEST"))
			{
				temp = skype_send_message("GET MESSAGE %s PARTNER_HANDLE", msg_num);
				sender = g_strdup(&temp[24+strlen(msg_num)]);
				g_free(temp);
				temp = skype_send_message("GET MESSAGE %s BODY", msg_num);
				body = g_strdup(&temp[14+strlen(msg_num)]);
				g_free(temp);
				temp = skype_send_message("GET MESSAGE %s TIMESTAMP", msg_num);
				mtime = atoi(&temp[19+strlen(msg_num)]);
				g_free(temp);
				
				/* Escape the body to HTML */
				body_html = skype_strdup_withhtml(body);
				g_free(body);

				if (g_str_equal(type, "TEXT"))
				{
					if (g_str_equal(sender, my_username))
					{
						temp = skype_send_message("GET CHATMESSAGE %s CHATNAME", msg_num);
						chatname = g_strdup(&temp[18+strlen(msg_num)]);
						g_free(temp);
						//skype_debug_info("skype", "Chatname: '%s'\n", chatname);
						chatusers = g_strsplit_set(chatname, "/;", 3);
						if (g_str_equal(&chatusers[0][1], my_username))
							sender = &chatusers[1][1];
						else
							sender = &chatusers[0][1];
						serv_got_im(gc, sender, body_html, PURPLE_MESSAGE_SEND, mtime);
						g_strfreev(chatusers);
					} else {
						serv_got_im(gc, sender, body_html, PURPLE_MESSAGE_RECV, mtime);
					}
				}/* else if (g_str_equal(type, "AUTHREQUEST") && !g_str_equal(sender, my_username))
				{
					skype_debug_info("User %s requested alternate authorisation\n", sender);
					purple_account_request_authorization(this_account, sender, NULL, skype_get_user_info(sender, "FULLNAME"),
												body, (purple_find_buddy(this_account, sender) != NULL),
												skype_auth_allow, skype_auth_deny, (gpointer)g_strdup(sender));
				}*/

				skype_send_message("SET MESSAGE %s SEEN", msg_num);
			}
		} else if (g_str_equal(string_parts[3], "SENT"))
		{
			/* mark it as seen, to remove notification from skype ui */
			
			/* dont async this -> infinite loop */
			skype_send_message("SET MESSAGE %s SEEN", string_parts[1]);
		}
	} else if (g_str_equal(command, "CHATMESSAGE"))
	{
		if ((g_str_equal(string_parts[3], "RECEIVED"))
			|| (g_str_equal(string_parts[3], "SENT"))
			)
		{
			if (messages_table == NULL)
			{
				messages_table = g_hash_table_new(NULL, NULL);
			}
			skypemessage = g_new0(SkypeMessage, 1);
			skypemessage->account = this_account;
			if (g_str_equal(string_parts[3], "RECEIVED"))
				skypemessage->flags = PURPLE_MESSAGE_RECV;
			else if (g_str_equal(string_parts[3], "SENT"))
				skypemessage->flags = PURPLE_MESSAGE_SEND;
			g_hash_table_insert(messages_table, GINT_TO_POINTER(atoi(string_parts[1])), skypemessage);
//			printf("Message %s has int %d (%d)\n", string_parts[1], atoi(string_parts[1]), GINT_TO_POINTER(atoi(string_parts[1])));
			skype_get_chatmessage_info(atoi(string_parts[1]));
		} else if (g_str_equal(string_parts[2], "TYPE"))
		{
			skypemessage = g_hash_table_lookup(messages_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (skypemessage != NULL)
			{
				//try to keep these in order of most likely to least likely
				if (g_str_equal(string_parts[3], "SAID") ||
							g_str_equal(string_parts[3], "TEXT"))
				{
					skypemessage->type = SKYPE_MESSAGE_TEXT;
				} else if (g_str_equal(string_parts[3], "EMOTED"))
				{
					skypemessage->type = SKYPE_MESSAGE_EMOTE;
				} else if (g_str_equal(string_parts[3], "ADDEDMEMBERS"))
				{
					skypemessage->type = SKYPE_MESSAGE_ADD;
				} else if (g_str_equal(string_parts[3], "LEFT"))
				{
					skypemessage->type = SKYPE_MESSAGE_LEFT;
				} else if (g_str_equal(string_parts[3], "KICKED") ||
							g_str_equal(string_parts[3], "KICKBANNED"))
				{
					skypemessage->type = SKYPE_MESSAGE_KICKED;
				} else if (g_str_equal(string_parts[3], "SETTOPIC"))
				{
					skypemessage->type = SKYPE_MESSAGE_TOPIC;
				} else {
					skypemessage->type = SKYPE_MESSAGE_OTHER;	
				}
			} else {
				skype_debug_info("skype", "Skype message %s not in hashtable\n", string_parts[1]);
			}
		} else if (g_str_equal(string_parts[2], "CHATNAME"))
		{
			skypemessage = g_hash_table_lookup(messages_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (skypemessage != NULL)
			{
				skypemessage->chatname = g_strdup(string_parts[3]);
			} else {
				skype_debug_info("skype", "Skype message %s not in hashtable\n", string_parts[1]);
			}
		} else if (g_str_equal(string_parts[2], "BODY"))
		{
			skypemessage = g_hash_table_lookup(messages_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (skypemessage != NULL)
			{
				skypemessage->body = g_strdup(string_parts[3]);
			} else {
				skype_debug_info("skype", "Skype message %s not in hashtable\n", string_parts[1]);
			}
		} else if (g_str_equal(string_parts[2], "FROM_HANDLE"))
		{
			skypemessage = g_hash_table_lookup(messages_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (skypemessage != NULL)
			{
				skypemessage->from_handle = g_strdup(string_parts[3]);
			} else {
				skype_debug_info("skype", "Skype message %s not in hashtable\n", string_parts[1]);
			}
		} else if (g_str_equal(string_parts[2], "USERS"))
		{
			skypemessage = g_hash_table_lookup(messages_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (skypemessage != NULL)
			{
				skypemessage->users = g_strsplit(string_parts[3], " ", -1);
			} else {
				skype_debug_info("skype", "Skype message %s not in hashtable\n", string_parts[1]);
			}
		} else if (g_str_equal(string_parts[2], "LEAVEREASON"))
		{
			skypemessage = g_hash_table_lookup(messages_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (skypemessage != NULL)
			{
				skypemessage->leavereason = g_strdup(string_parts[3]);
			} else {
				skype_debug_info("skype", "Skype message %s not in hashtable\n", string_parts[1]);
			}
		} else if (g_str_equal(string_parts[2], "TIMESTAMP"))
		{
			skypemessage = g_hash_table_lookup(messages_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (skypemessage != NULL)
			{
				skypemessage->timestamp = atoi(string_parts[3]);
			} else {
				skype_debug_info("skype", "Skype message %s not in hashtable\n", string_parts[1]);
			}
		}
		
		handle_complete_message(atoi(string_parts[1]));
	} else if (g_str_equal(command, "CHAT"))
	{
		//find the matching chat to update
		chat = skype_find_chat(string_parts[1], this_account);
		if (g_str_equal(string_parts[2], "TYPE"))
		{
			if (g_str_equal(string_parts[3], "DIALOG") || g_str_equal(string_parts[3], "LEGACY_DIALOG"))
			{
				chat->type = PURPLE_CONV_TYPE_IM;
			} else {
				chat->type = PURPLE_CONV_TYPE_CHAT;
			}
		} else if (g_str_equal(string_parts[2], "MEMBERS"))
		{
			chatusers = g_strsplit(string_parts[3], " ", 0);
			if (chat->members)
				g_strfreev(chat->members);
			chat->members = chatusers;
			if (chat->type == PURPLE_CONV_TYPE_CHAT)
			{
				purple_conv_chat_clear_users(PURPLE_CONV_CHAT(chat->conv));
				for (i=0; chatusers[i]; i++)
				{
					purple_conv_chat_add_user(PURPLE_CONV_CHAT(chat->conv), chatusers[i], NULL, PURPLE_CBFLAGS_NONE, FALSE);
				}
			}
		} else if (chat->conv && g_str_equal(string_parts[2], "FRIENDLYNAME"))
		{
			if (chat->type == PURPLE_CONV_TYPE_CHAT)
			{
				purple_conversation_set_title(chat->conv, string_parts[3]);
				purple_conversation_update(chat->conv, PURPLE_CONV_UPDATE_TITLE);
			}
		} else if (chat->conv && g_str_equal(string_parts[2], "TOPIC"))
		{
			if (chat->type == PURPLE_CONV_TYPE_CHAT)
			{
				purple_conv_chat_set_topic(PURPLE_CONV_CHAT(chat->conv), my_username, string_parts[3]);
				purple_conversation_update(chat->conv, PURPLE_CONV_UPDATE_TOPIC);
			}
		}
		chat = skype_find_chat(string_parts[1], this_account);
	} else if (g_str_equal(command, "FILETRANSFER"))
	{
		//lookup current file transfers to see if there's already one there
		glist_temp = g_list_find_custom(purple_xfers_get_all(),
										string_parts[1],
										(GCompareFunc)skype_find_filetransfer);
		if (glist_temp == NULL && g_str_equal(string_parts[2], "TYPE"))
		{
			temp = skype_send_message("GET FILETRANSFER %s PARTNER_HANDLE", string_parts[1]);
			sender = g_strdup(&temp[29+strlen(string_parts[1])]);
			g_free(temp);
			if (g_str_equal(string_parts[3], "INCOMING"))
			{
				transfer = purple_xfer_new(this_account, PURPLE_XFER_RECEIVE, sender);
			} else {
				transfer = purple_xfer_new(this_account, PURPLE_XFER_SEND, sender);
			}
			transfer->data = g_strdup(string_parts[1]);
			purple_xfer_set_init_fnc(transfer, skype_accept_transfer);
			purple_xfer_set_request_denied_fnc(transfer, skype_decline_transfer);
			temp = skype_send_message("GET FILETRANSFER %s FILENAME", string_parts[1]);
			skype_debug_info("skype", "Filename: '%s'\n", &temp[23+strlen(string_parts[1])]);
			purple_xfer_set_filename(transfer, g_strdup(&temp[23+strlen(string_parts[1])]));
			g_free(temp);
			temp = skype_send_message("GET FILETRANSFER %s FILEPATH", string_parts[1]);
			if (strlen(&temp[23+strlen(string_parts[1])]))
				purple_xfer_set_local_filename(transfer, g_strdup(&temp[23+strlen(string_parts[1])]));
			else
				purple_xfer_set_local_filename(transfer, purple_xfer_get_filename(transfer));
			g_free(temp);
			temp = skype_send_message("GET FILETRANSFER %s FILESIZE", string_parts[1]);
			purple_xfer_set_size(transfer, atol(&temp[23+strlen(string_parts[1])]));
			g_free(temp);
			purple_xfer_add(transfer);
		} else if (glist_temp != NULL) {
			transfer = glist_temp->data;
		}
		if (transfer != NULL)
		{
			/*if (g_str_equal(string_parts[2], "TYPE"))
			{
				if (g_str_equal(string_parts[3], "INCOMING"))
				{
					transfer->type = PURPLE_XFER_RECEIVE;
				} else {
					transfer->type = PURPLE_XFER_SEND;
				}
			} else if (g_str_equal(string_parts[2], "PARTNER_HANDLE"))
			{
				transfer->who = g_strdup(string_parts[3]);
			} else*/ if (g_str_equal(string_parts[2], "FILENAME"))
			{
				purple_xfer_set_filename(transfer, string_parts[3]);
			} else if (g_str_equal(string_parts[2], "FILEPATH"))
			{
				if (strlen(string_parts[3]))
					purple_xfer_set_local_filename(transfer, string_parts[3]);
			} else if (g_str_equal(string_parts[2], "STATUS"))
			{
				if (g_str_equal(string_parts[3], "NEW") ||
					g_str_equal(string_parts[3], "WAITING_FOR_ACCEPT"))
				{
					//Skype API doesn't let us accept transfers
					//purple_xfer_request(transfer);
					if (purple_xfer_get_type(transfer) == PURPLE_XFER_RECEIVE)
					{
#						ifndef __APPLE__
							skype_send_message_nowait("OPEN FILETRANSFER");
#						else
							purple_notify_info(this_account, _("File Transfers"), g_strdup_printf(_("%s wants to send you a file"), purple_xfer_get_remote_user(transfer)), NULL);
#						endif
						purple_xfer_conversation_write(transfer, g_strdup_printf(_("%s wants to send you a file"), purple_xfer_get_remote_user(transfer)), FALSE);
					}
					purple_xfer_set_status(transfer, PURPLE_XFER_STATUS_NOT_STARTED);
				} else if (g_str_equal(string_parts[3], "COMPLETED"))
				{
					purple_xfer_set_completed(transfer, TRUE);
				} else if (g_str_equal(string_parts[3], "CONNECTING") ||
							g_str_equal(string_parts[3], "TRANSFERRING") ||
							g_str_equal(string_parts[3], "TRANSFERRING_OVER_RELAY"))
				{
					purple_xfer_set_status(transfer, PURPLE_XFER_STATUS_STARTED);
					transfer->start_time = time(NULL);
				} else if (g_str_equal(string_parts[3], "CANCELLED"))
				{
					//transfer->end_time = time(NULL);
					//transfer->bytes_remaining = 0;
					//purple_xfer_set_status(transfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
					purple_xfer_cancel_local(transfer);
				}/* else if (g_str_equal(string_parts[3], "FAILED"))
				{
					//transfer->end_time = time(NULL);
					//purple_xfer_set_status(transfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
					purple_xfer_cancel_remote(transfer);
				}*/
				purple_xfer_update_progress(transfer);
			} else if (g_str_equal(string_parts[2], "STARTTIME"))
			{
				transfer->start_time = atol(string_parts[3]);
				purple_xfer_update_progress(transfer);
			/*} else if (g_str_equal(string_parts[2], "FINISHTIME"))
			{
				if (!g_str_equal(string_parts[3], "0"))
					transfer->end_time = atol(string_parts[3]);
				purple_xfer_update_progress(transfer);*/
			} else if (g_str_equal(string_parts[2], "BYTESTRANSFERRED"))
			{
				purple_xfer_set_bytes_sent(transfer, atol(string_parts[3]));
				purple_xfer_update_progress(transfer);
			} else if (g_str_equal(string_parts[2], "FILESIZE"))
			{
				purple_xfer_set_size(transfer, atol(string_parts[3]));
			} else if (g_str_equal(string_parts[2], "FAILUREREASON") &&
						!g_str_equal(string_parts[3], "UNKNOWN"))
			{
				temp = NULL;
				if (g_str_equal(string_parts[3], "SENDER_NOT_AUTHORIZED"))
				{
					temp = g_strdup(_("Not Authorized"));
				} else if (g_str_equal(string_parts[3], "REMOTELY_CANCELLED"))
				{
					purple_xfer_cancel_remote(transfer);
					purple_xfer_update_progress(transfer);
				} else if (g_str_equal(string_parts[3], "FAILED_READ"))
				{
					temp = g_strdup(_("Read error"));
				} else if (g_str_equal(string_parts[3], "FAILED_REMOTE_READ"))
				{
					temp = g_strdup(_("Read error"));
				} else if (g_str_equal(string_parts[3], "FAILED_WRITE"))
				{
					temp = g_strdup(_("Write error"));
				} else if (g_str_equal(string_parts[3], "FAILED_REMOTE_WRITE"))
				{
					temp = g_strdup(_("Write error"));
				} else if (g_str_equal(string_parts[3], "REMOTE_DOES_NOT_SUPPORT_FT"))
				{
					temp = g_strdup_printf(_("Unable to send file to %s, user does not support file transfers"), transfer->who);
				} else if (g_str_equal(string_parts[3], "REMOTE_OFFLINE_FOR_TOO_LONG"))
				{
					temp = g_strdup(_("Recipient Unavailable"));
				}
				if (temp && strlen(temp))
				{
					purple_xfer_error(transfer->type, this_account, transfer->who, temp);
					g_free(temp);
				}
			}
		}
	} else if (g_str_equal(command, "WINDOWSTATE"))
	{
		if (g_str_equal(string_parts[1], "HIDDEN"))
		{
			skype_send_message_nowait("SET SILENT_MODE ON");
		}
	} else if (g_str_equal(command, "PROFILE"))
	{
		if (g_str_equal(string_parts[1], "FULLNAME"))
		{
			temp = g_strconcat(string_parts[2], " ", string_parts[3], NULL);
			//this is the full name of the logged in user... useful for the account name
			g_free(temp);
		}
	} else if (g_str_equal(command, "CURRENTUSERHANDLE"))
	{
		//the currently logged in username is at string_parts[1]
	} else if (g_str_equal(command, "GROUPS"))
	{
		if (groups_table == NULL)
			groups_table = g_hash_table_new(NULL, NULL);
		chatusers = g_strsplit(strchr(message, ' ')+1, ", ", 0);
		for(i = 0; chatusers[i]; i++)
		{
			skype_send_message_nowait("GET GROUP %s DISPLAYNAME", chatusers[i]);
			skype_send_message_nowait("GET GROUP %s USERS", chatusers[i]);
		}
		g_strfreev(chatusers);
	} else if (g_str_equal(command, "GROUP"))
	{
		if (groups_table == NULL)
			groups_table = g_hash_table_new(NULL, NULL);
		//TODO Handle Group stuff:
		//  Messages from skype to move users in to/out of a group
		if (g_str_equal(string_parts[2], "DISPLAYNAME"))
		{
			temp_group = g_hash_table_lookup(groups_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (!temp_group)
			{
				temp_group = purple_find_group(string_parts[3]);
				if (!temp_group)
				{
					temp_group = purple_group_new(string_parts[3]);
					purple_blist_add_group(temp_group, NULL);
				}
				purple_blist_node_set_int(&temp_group->node, "skype_group_number", atoi(string_parts[1]));
				g_hash_table_insert(groups_table, GINT_TO_POINTER(atoi(string_parts[1])), temp_group);
			}
		} else if (g_str_equal(string_parts[2], "USERS"))
		{
			temp_group = g_hash_table_lookup(groups_table, GINT_TO_POINTER(atoi(string_parts[1])));
			if (temp_group && string_parts[3])
			{
				chatusers = g_strsplit(string_parts[3], ", ", -1);
				for (i = 0; chatusers[i]; i++)
				{
					buddy = purple_find_buddy(this_account, chatusers[i]);
					if (buddy && temp_group != purple_buddy_get_group(buddy))
						purple_blist_add_buddy(buddy, NULL, temp_group, NULL);
				}
				g_strfreev(chatusers);	
			}		
		}
	} else if (g_str_equal(command, "APPLICATION") && 
				g_str_equal(string_parts[1], "libpurple_typing"))
	{
		if (g_str_equal(string_parts[2], "DATAGRAM"))
		{
			chatusers = g_strsplit_set(string_parts[3], ": ", 3);
			sender = chatusers[0];
			temp = chatusers[2];
			if (sender != NULL && temp != NULL)
			{
				sender = g_strdup(sender);
				temp = g_strdup(temp);
				if (g_str_equal(temp, "PURPLE_NOT_TYPING"))
					serv_got_typing(gc, sender, 10, PURPLE_NOT_TYPING);
				else if (g_str_equal(temp, "PURPLE_TYPING"))
					serv_got_typing(gc, sender, 10, PURPLE_TYPING);
				else if (g_str_equal(temp, "PURPLE_TYPED"))
					serv_got_typing(gc, sender, 10, PURPLE_TYPED);
				g_free(sender);
				g_free(temp);
			}
			g_strfreev(chatusers);
		} else if (g_str_equal(string_parts[2], "STREAMS"))
		{
			chatusers = g_strsplit_set(string_parts[3], ": ", -1);
			for(i=0; chatusers[i] && chatusers[i+1]; i+=2)
			{
				temp = g_strconcat("stream-", chatusers[i], NULL);
				purple_account_set_string(this_account, temp, chatusers[i+1]);
				g_free(temp);
			}
			g_strfreev(chatusers);
		}
#ifdef USE_VV
	} else if (g_str_equal(command, "CALL"))
	{
		if (g_str_equal(string_parts[2], "STATUS"))
		{ 
			if (g_str_equal(string_parts[3], "RINGING"))
			{
				skype_handle_incoming_call(gc, string_parts[1]);
			} else if (g_str_equal(string_parts[3], "FINISHED") ||
						g_str_equal(string_parts[3], "CANCELLED") ||
						g_str_equal(string_parts[3], "FAILED"))
			{
				skype_handle_call_got_ended(string_parts[1]);
			}
		}
#else
	} else if (g_str_equal(command, "CALL"))
	{
		if (g_str_equal(string_parts[2], "STATUS") &&
			g_str_equal(string_parts[3], "RINGING"))
		{
			temp = skype_send_message("GET CALL %s TYPE", string_parts[1]);
			type = g_new0(gchar, 9);
			sscanf(temp, "CALL %*s TYPE %[^_]", type);
			g_free(temp);
			temp = skype_send_message("GET CALL %s PARTNER_HANDLE", string_parts[1]);
			sender = g_strdup(&temp[21+strlen(string_parts[1])]);
			g_free(temp);
			if (g_str_equal(type, "INCOMING"))
			{
				temp = g_strdup_printf(_("%s is calling you."), sender);
				purple_request_action(gc, _("Incoming Call"), temp, 
									  _("Do you want to accept their call?"),
								0, this_account, sender, NULL, g_strdup(string_parts[1]), 2, 
								_("_Accept"), G_CALLBACK(skype_call_accept_cb), 
									  _("_Reject"), G_CALLBACK(skype_call_reject_cb),
									  _("_Ignore"), G_CALLBACK(skype_call_ignore_cb),
									  _("Send to _Voicemail"), G_CALLBACK(skype_call_voicemail_cb),
									  _("_Forward"), G_CALLBACK(skype_call_forward_cb));
				g_free(temp);
			}
			g_free(sender);
			g_free(type);
		}
#endif	
	} else if (g_str_equal(command, "SMS"))
	{
		skype_debug_info("skype", "SMS lookup table %x\n", sms_convo_link_table);
		if (sms_convo_link_table != NULL)
		{
			temp = g_hash_table_lookup(sms_convo_link_table, string_parts[1]);
			skype_debug_info("skype", "Found mobile %s from SMS number %s\n", temp?temp:"null", string_parts[1]);
			conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, temp, this_account);
			skype_debug_info("skype", "Found conv %d\n", conv);
			if (conv)
			{
				if (g_str_equal(string_parts[2], "STATUS"))
				{
					temp = g_strconcat(_("Status: "), string_parts[3], NULL);
					purple_conversation_write(conv, NULL, temp, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG, time(NULL));
					g_free(temp);
				} else if (g_str_equal(string_parts[2], "FAILUREREASON"))
				{
					temp = g_strconcat(_("Failure Reason: "), string_parts[3], NULL);
					purple_conversation_write(conv, NULL, temp, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG, time(NULL));
					g_free(temp);
					skype_send_message_nowait("SET SMS %s SEEN", string_parts[1]);
				} else if (g_str_equal(string_parts[2], "PRICE"))
				{
					if (atoi(string_parts[3]) > 0 &&
						purple_conversation_get_data(conv, "price_precision") &&
						purple_conversation_get_data(conv, "price_currency"))
					{
						int exponenet = 1;
						double d;
						for (i = atoi(purple_conversation_get_data(conv, "price_precision"));
							i > 0; i--)
						{
							exponenet *= 10;
						}
						d = atof(string_parts[3]) / exponenet;
						temp = g_strdup_printf("%s %s %f", _("Price: "),
											(char*)purple_conversation_get_data(conv, "price_currency"),
											d);
						purple_conversation_write(conv, NULL, temp, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG, time(NULL));
						g_free(temp);
					}
				} else if (g_str_equal(string_parts[2], "PRICE_PRECISION"))
				{
					purple_conversation_set_data(conv, "price_precision", g_strdup(string_parts[3]));
				} else if (g_str_equal(string_parts[2], "PRICE_CURRENCY"))
				{
					purple_conversation_set_data(conv, "price_currency", g_strdup(string_parts[3]));
				}
			}
		}
	}
	if (string_parts)
	{
		g_strfreev(string_parts);
	}
	if (message)
	{
		g_free(message);
	}
	return FALSE;
}

void
skype_call_accept_cb(gchar *call)
{
	skype_send_message_nowait("ALTER CALL %s ANSWER", call);
	skype_send_message_nowait("SET CALL %s SEEN", call);
	g_free(call);
}

void
skype_call_reject_cb(gchar *call)
{
	skype_send_message_nowait("ALTER CALL %s END HANGUP", call);
	skype_send_message_nowait("SET CALL %s SEEN", call);
	g_free(call);
}

void
skype_call_ignore_cb(gchar *call)
{
	skype_send_message_nowait("SET CALL %s SEEN", call);
	g_free(call);
}

void
skype_call_voicemail_cb(gchar *call)
{
	skype_send_message_nowait("ALTER CALL %s END REDIRECT_TO_VOICEMAIL", call);
	skype_send_message_nowait("SET CALL %s SEEN", call);
	g_free(call);
}

void
skype_call_forward_cb(gchar *call)
{
	skype_send_message_nowait("ALTER CALL %s END FORWARD_CALL", call);
	skype_send_message_nowait("SET CALL %s SEEN", call);
	g_free(call);
}

void
skype_auth_allow(gpointer sender)
{
	skype_send_message("SET USER %s ISAUTHORIZED TRUE", sender);
	g_free(sender);
}

void
skype_auth_deny(gpointer sender)
{
	skype_send_message("SET USER %s ISAUTHORIZED FALSE", sender);
	g_free(sender);
}

gint
skype_find_filetransfer(PurpleXfer *transfer, char *skypeid)
{
	if (transfer == NULL || transfer->data == NULL || skypeid == NULL)
		return -1;
	return strcmp(transfer->data, skypeid);
}

void
skype_accept_transfer(PurpleXfer *transfer)
{
	//can't accept transfers
}

void
skype_decline_transfer(PurpleXfer *transfer)
{
	//can't reject transfers
}

SkypeChat*
skype_find_chat(const gchar *chat_id, PurpleAccount *this_account)
{
	SkypeChat *chat;
	int i;
	
	if (chat_id == NULL)
		return NULL;
	
	if(chat_link_table == NULL)
	{
		chat_link_table = g_hash_table_new(g_str_hash, g_str_equal);
	}
	
	chat = g_hash_table_lookup(chat_link_table, chat_id);
	if (chat == NULL)
	{
		chat = g_new0(SkypeChat, 1);
		chat->name = g_strdup(chat_id);
		chat->account = this_account;
		g_hash_table_insert(chat_link_table, (char*)chat_id, chat);
		
		skype_send_message_nowait("GET CHAT %s TYPE", chat_id);
		skype_send_message_nowait("GET CHAT %s MEMBERS", chat_id);
		skype_send_message_nowait("GET CHAT %s FRIENDLYNAME", chat_id);
		skype_send_message_nowait("GET CHAT %s TOPIC", chat_id);
	}
	
	chat->conv = NULL;
	
	if (chat->type)
	{
		if (chat->type == PURPLE_CONV_TYPE_CHAT)
		{
			chat->conv = purple_find_conversation_with_account(chat->type, chat_id, this_account);
			if (!chat->conv)
			{
				chat->prpl_chat_id = g_str_hash(chat_id);
				chat->conv = serv_got_joined_chat(this_account->gc, chat->prpl_chat_id, chat_id);
			}
		} else if (chat->type == PURPLE_CONV_TYPE_IM)
		{
			if (!chat->partner_handle && chat->members)
			{
				for (i=0; chat->members[i]; i++)
				{
					if (*chat->members[i] != '\0' && !g_str_equal(chat->members[i], skype_get_account_username(chat->account)))
					{
						chat->partner_handle = g_strdup(chat->members[i]);
						break;
					}
				}
			}
			if (chat->partner_handle)
			{
				chat->conv = purple_find_conversation_with_account(chat->type, chat->partner_handle, chat->account);
				if (!chat->conv)
					chat->conv = purple_conversation_new(chat->type, chat->account, chat->partner_handle);
			}
		}
		if (chat->conv)
			purple_conversation_set_data(chat->conv, "chat_id", g_strdup(chat_id));
	}
	
	
	return chat;
}

gint
skype_find_chat_compare_func(PurpleConversation *conv, char *chat_id)
{
	char *lookup;
	if (chat_id == NULL || conv == NULL || conv->data == NULL)
		return -1;
	lookup = purple_conversation_get_data(conv, "chat_id");
	if (lookup == NULL)
		return -1;
	return strcmp(lookup, chat_id);
}

/* Since this function isn't public, and we need it to be, redefine it here */
static void
purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatusType status)
{
	g_return_if_fail(xfer != NULL);

	if(xfer->type == PURPLE_XFER_SEND) {
		switch(status) {
			case PURPLE_XFER_STATUS_ACCEPTED:
				purple_signal_emit(purple_xfers_get_handle(), "file-send-accept", xfer);
				break;
			case PURPLE_XFER_STATUS_STARTED:
				purple_signal_emit(purple_xfers_get_handle(), "file-send-start", xfer);
				break;
			case PURPLE_XFER_STATUS_DONE:
				purple_signal_emit(purple_xfers_get_handle(), "file-send-complete", xfer);
				break;
			case PURPLE_XFER_STATUS_CANCEL_LOCAL:
			case PURPLE_XFER_STATUS_CANCEL_REMOTE:
				purple_signal_emit(purple_xfers_get_handle(), "file-send-cancel", xfer);
				break;
			default:
				break;
		}
	} else if(xfer->type == PURPLE_XFER_RECEIVE) {
		switch(status) {
			case PURPLE_XFER_STATUS_ACCEPTED:
				purple_signal_emit(purple_xfers_get_handle(), "file-recv-accept", xfer);
				break;
			case PURPLE_XFER_STATUS_STARTED:
				purple_signal_emit(purple_xfers_get_handle(), "file-recv-start", xfer);
				break;
			case PURPLE_XFER_STATUS_DONE:
				purple_signal_emit(purple_xfers_get_handle(), "file-recv-complete", xfer);
				break;
			case PURPLE_XFER_STATUS_CANCEL_LOCAL:
			case PURPLE_XFER_STATUS_CANCEL_REMOTE:
				purple_signal_emit(purple_xfers_get_handle(), "file-recv-cancel", xfer);
				break;
			default:
				break;
		}
	}

	xfer->status = status;
}


gboolean
skype_sync_skype_close(PurpleConnection *gc)
{
	if (gc != NULL)
		purple_connection_error(gc, _("\nSkype program closed"));
	return FALSE;
}

gboolean
handle_complete_message(int messagenumber)
{
	SkypeMessage *skypemessage = NULL;
	SkypeChat *chat = NULL;
	gchar *body_html = NULL;
	int i;

	if (messages_table == NULL)
		return FALSE;
	
	skypemessage = g_hash_table_lookup(messages_table, GINT_TO_POINTER(messagenumber));
	if (skypemessage == NULL)
		return FALSE; //Message no longer exists, must have delt with it already
	
	if (!skypemessage->chatname || !skypemessage->type || !skypemessage->account)
		return FALSE; //Haven't finished filling in all the required details
	
	chat = skype_find_chat(skypemessage->chatname, skypemessage->account);
	if (!chat->type)
	{
		skype_debug_info("skype", "Chat %s has no type\n", skypemessage->chatname);
		//dont know where to put this message
		skype_send_message_nowait("GET CHAT %s TYPE", skypemessage->chatname);
		chat->type_request_count++;
		//Only try a maximum of 100 times to prevent an infinite loop (in case the chat name is unknown)
		if (chat->type_request_count < 100)
		{
			//just wait for a second for the chat to be updated
			purple_timeout_add_seconds(1, (GSourceFunc)handle_complete_message, GINT_TO_POINTER(messagenumber));
		}
		return FALSE;
	}
	
	switch(skypemessage->type)
	{
		case SKYPE_MESSAGE_UNSET:
			return FALSE;
		case SKYPE_MESSAGE_EMOTE:
			if (!skypemessage->body)
				return FALSE;
			body_html = g_strdup_printf("/me %s", skypemessage->body);
			g_free(skypemessage->body);
			skypemessage->body = body_html;
			//set it to be text so that we dont do it again
			skypemessage->type = SKYPE_MESSAGE_TEXT;
			//fallthrough intentional
		case SKYPE_MESSAGE_OTHER:
			//'other' type of message is generally an auth request
		case SKYPE_MESSAGE_TEXT:
			if (!skypemessage->body || !skypemessage->from_handle || !skypemessage->timestamp)
				return FALSE;
			body_html = skype_strdup_withhtml(skypemessage->body);
			if (chat->type == PURPLE_CONV_TYPE_CHAT)
			{
				if (skypemessage->flags != PURPLE_MESSAGE_SEND)
				{
					if (chat->prpl_chat_id)
						i = chat->prpl_chat_id;
					else
						i = g_str_hash(chat->name);
					serv_got_chat_in(skypemessage->account->gc, i, skypemessage->from_handle, skypemessage->flags, body_html, skypemessage->timestamp);
				}
			} else if (chat->type == PURPLE_CONV_TYPE_IM)
			{
				if (skypemessage->flags != PURPLE_MESSAGE_SEND)
				{
					PurpleAccount *acct = skypemessage->account;
					if (!g_str_equal(skypemessage->from_handle, skype_get_account_username(acct)))
					{
						serv_got_im(acct->gc, skypemessage->from_handle, body_html, skypemessage->flags, skypemessage->timestamp);
					} else if (chat->partner_handle)
					{
						//if we're here, then we're receiving a message that we sent from a different computer
						serv_got_im(acct->gc, chat->partner_handle, body_html, PURPLE_MESSAGE_SEND, skypemessage->timestamp);
					} else {
						//use the chat name to work out who it came from
						//in format #username1/$username2;junktext for IM's
						char *start, *end;
						start = strchr(skypemessage->chatname, '#');
						if (start)
						{
							start += 1;
							end = strchr(start, '/');
							if (end)
							{
								start = g_strndup(start, end-start);
								if (!g_str_equal(skype_get_account_username(acct), start))
								{
									serv_got_im(acct->gc, start, body_html, PURPLE_MESSAGE_SEND, skypemessage->timestamp);
									g_free(start);
									start = (char *) 1;
								}
								else
								{
									g_free(start);
									start = NULL;
								}
							} else {
								start = NULL;
							}
						}
						if (!start)
						{
							start = strchr(skypemessage->chatname, '$');
							if (start)
							{
								start += 1;
								end = strchr(start, ';');
								if (end)
								{
									start = g_strndup(start, end-start);
									if (!g_str_equal(skype_get_account_username(acct), start))
									{
										serv_got_im(acct->gc, start, body_html, PURPLE_MESSAGE_SEND, skypemessage->timestamp);
										g_free(start);
									}
								}
							}
						}
					}
				}
			}
			break;
		case SKYPE_MESSAGE_LEFT:
			if (!skypemessage->from_handle || !skypemessage->leavereason)
				return FALSE;
			if (chat->conv && chat->type == PURPLE_CONV_TYPE_CHAT)
			{
				if (g_str_equal(skypemessage->from_handle, skype_get_account_username(skypemessage->account)))
					purple_conv_chat_left(PURPLE_CONV_CHAT(chat->conv));
				purple_conv_chat_remove_user(PURPLE_CONV_CHAT(chat->conv), skypemessage->from_handle, skypemessage->leavereason);
			}
			break;
		case SKYPE_MESSAGE_ADD:
			if (!skypemessage->users)
				return FALSE;
			if (chat->conv && chat->type == PURPLE_CONV_TYPE_CHAT)
			{
				for (i=0; skypemessage->users[i]; i++)
					if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(chat->conv), skypemessage->users[i]))
						purple_conv_chat_add_user(PURPLE_CONV_CHAT(chat->conv), skypemessage->users[i], NULL, PURPLE_CBFLAGS_NONE, TRUE);	
			}
			break;
		case SKYPE_MESSAGE_KICKED:
			if (!skypemessage->users)
				return FALSE;
			if (chat->conv && chat->type == PURPLE_CONV_TYPE_CHAT)
			{
				for (i=0; skypemessage->users[i]; i++)
				{
					if (skypemessage->from_handle)
						purple_conv_chat_remove_user(PURPLE_CONV_CHAT(chat->conv), skypemessage->users[i], g_strdup_printf("Kicked by %s", skypemessage->from_handle));
					else
						purple_conv_chat_remove_user(PURPLE_CONV_CHAT(chat->conv), skypemessage->users[i], g_strdup("Kicked"));
				}
			}
			break;
		case SKYPE_MESSAGE_TOPIC:
			if (!skypemessage->body || !skypemessage->from_handle)
				return FALSE;
			if (chat->conv && chat->type == PURPLE_CONV_TYPE_CHAT)
			{
				purple_conv_chat_set_topic(PURPLE_CONV_CHAT(chat->conv), skypemessage->from_handle, skypemessage->body);
				serv_got_chat_in(skypemessage->account->gc, purple_conv_chat_get_id(PURPLE_CONV_CHAT(chat->conv)), skypemessage->from_handle, PURPLE_MESSAGE_SYSTEM, skype_strdup_withhtml(g_strdup_printf(_("%s has changed the topic to: %s"), skypemessage->from_handle, skypemessage->body)), time(NULL));
				purple_conversation_update(chat->conv, PURPLE_CONV_UPDATE_TOPIC);
			} 
			break;
	}
	
	if (skypemessage->flags == PURPLE_MESSAGE_RECV)
		skype_send_message_nowait("SET CHATMESSAGE %d SEEN", messagenumber);
	if (g_hash_table_remove(messages_table, GINT_TO_POINTER(messagenumber)))
	{
		//free the message here
		skypemessage->type = 0;
		skypemessage->timestamp = 0;
		if (skypemessage->chatname)
		{
			g_free(skypemessage->chatname);
			skypemessage->chatname = NULL;
		}
		if (skypemessage->body)
		{
			g_free(skypemessage->body);
			skypemessage->body = NULL;
		}
		if (skypemessage->from_handle)
		{
			g_free(skypemessage->from_handle);
			skypemessage->from_handle = NULL;
		}
		if (skypemessage->users)
		{
			g_strfreev(skypemessage->users);
			skypemessage->users = NULL;
		}
		if (skypemessage->leavereason)
		{
			g_free(skypemessage->leavereason);
			skypemessage->leavereason = NULL;
		}
		
		g_free(skypemessage);
	}
	
	//can be used in eventloop
	return FALSE;
}

