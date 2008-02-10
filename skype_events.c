#include <ft.h>

void skype_auth_allow(gpointer sender);
void skype_auth_deny(gpointer sender);
static gboolean skype_handle_received_message(char *message);
gint skype_find_filetransfer(PurpleXfer *transfer, char *skypeid);
void skype_accept_transfer(PurpleXfer *transfer);
void skype_decline_transfer(PurpleXfer *transfer);
gint skype_find_chat(PurpleConversation *conv, char *chat_id);
static void purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatusType status);
void skype_call_accept_cb(gchar *call);
void skype_call_reject_cb(gchar *call);

gboolean skype_update_buddy_status(PurpleBuddy *buddy);
void skype_update_buddy_alias(PurpleBuddy *buddy);
void skype_update_buddy_icon(PurpleBuddy *buddy);
static PurpleAccount *skype_get_account(PurpleAccount *account);
char *skype_get_account_username(PurpleAccount *acct);
gchar *skype_get_user_info(const gchar *username, const gchar *property);
gchar *skype_strdup_withhtml(const gchar *src);

char *skype_send_message(char *message, ...);

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
	char *my_username;
	PurpleBuddy *buddy;
	char *body;
	char *body_html;
	char *msg_num;
	char *sender;
	char *type;
	int mtime;
	char *chatname;
	char *temp;
	char *chat_type;
	char **chatusers = NULL;
	PurpleXfer *transfer = NULL;
	PurpleConversation *conv = NULL;
	GList *glist_temp = NULL;
	int i;
	static int chat_count = 0;
	
	sscanf(message, "%s ", command);
	this_account = skype_get_account(NULL);
	if (this_account == NULL)
		return FALSE;
	gc = purple_account_get_connection(this_account);
	my_username = skype_get_account_username(this_account);
	string_parts = g_strsplit(message, " ", 4);
	
	if (strcmp(command, "USERSTATUS") == 0)
	{

	} else if (strcmp(command, "CONNSTATUS") == 0)
	{
		if (strcmp(string_parts[1], "LOGGEDOUT") == 0)
		{
			purple_connection_error(gc, _("\nSkype program closed"));
		}
	} else if ((strcmp(command, "USER") == 0) && (strcmp(string_parts[1], my_username) != 0))
	{
		buddy = purple_find_buddy(this_account, string_parts[1]);
		if (buddy != NULL)
		{
			if (strcmp(string_parts[2], "ONLINESTATUS") == 0)
			{
				skype_update_buddy_status(buddy);
				skype_update_buddy_icon(buddy);
			} else if (strcmp(string_parts[2], "MOOD_TEXT") == 0)
			{
				if (buddy->proto_data != NULL)
					g_free(buddy->proto_data);
				for (i=0; i<strlen(string_parts[3]); i++)
					if (string_parts[3][i] == '\n')
						string_parts[3][i] = ' ';
				buddy->proto_data = skype_strdup_withhtml(string_parts[3]);
			} else if (strcmp(string_parts[2], "DISPLAYNAME") == 0)
			{
				purple_blist_server_alias_buddy(buddy, g_strdup(string_parts[3]));
			} else if ((strcmp(string_parts[2], "BUDDYSTATUS") == 0) &&
					(strcmp(string_parts[3], "1") == 0))
			{
				purple_blist_remove_buddy(buddy);
			}
		} else if (strcmp(string_parts[2], "BUDDYSTATUS") == 0)
		{
			if (strcmp(string_parts[3], "3") == 0)
			{
				purple_debug_info("skype", "Buddy %s just got added\n", string_parts[1]);
				//buddy just got added.. handle it
				if (purple_find_buddy(this_account, string_parts[1]) == NULL)
				{
					purple_debug_info("skype", "Buddy not in list\n");
					buddy = purple_buddy_new(this_account, g_strdup(string_parts[1]), NULL);
					if (string_parts[1][0] == '+')
						purple_blist_add_buddy(buddy, NULL, purple_group_new("SkypeOut"), NULL);
					else
						purple_blist_add_buddy(buddy, NULL, purple_group_new("Skype"), NULL);
					skype_update_buddy_status(buddy);
					skype_update_buddy_alias(buddy);
					purple_prpl_got_user_idle(this_account, buddy->name, FALSE, 0);
					skype_update_buddy_icon(buddy);
				}
			}
		} else if (strcmp(string_parts[2], "RECEIVEDAUTHREQUEST") == 0)
		{
			//this event can be fired directly after authorising someone
			temp = skype_get_user_info(string_parts[1], "ISAUTHORIZED");
			if (strcmp(temp, "TRUE") != 0)
			{
				purple_debug_info("skype", "User %s requested authorisation\n", string_parts[1]);
				purple_account_request_authorization(this_account, string_parts[1], NULL, skype_get_user_info(string_parts[1], "FULLNAME"),
													string_parts[3], (purple_find_buddy(this_account, string_parts[1]) != NULL),
													skype_auth_allow, skype_auth_deny, (gpointer)g_strdup(string_parts[1]));
			}
			g_free(temp);
		}
	} else if (strcmp(command, "MESSAGE") == 0)
	{
		if (strcmp(string_parts[3], "RECEIVED") == 0)
		{
			msg_num = string_parts[1];
			temp = skype_send_message("GET MESSAGE %s TYPE", msg_num);
			type = g_strdup(&temp[14+strlen(msg_num)]);
			g_free(temp);
			if (strcmp(type, "TEXT") == 0 ||
				strcmp(type, "AUTHREQUEST") == 0)
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

				if (strcmp(type, "TEXT")==0)
				{
					if (strcmp(sender, my_username) == 0)
					{
						temp = skype_send_message("GET CHATMESSAGE %s CHATNAME", msg_num);
						chatname = g_strdup(&temp[18+strlen(msg_num)]);
						g_free(temp);
						//purple_debug_info("skype", "Chatname: '%s'\n", chatname);
						chatusers = g_strsplit_set(chatname, "/;", 3);
						if (strcmp(&chatusers[0][1], my_username) == 0)
							sender = &chatusers[1][1];
						else
							sender = &chatusers[0][1];
						serv_got_im(gc, sender, body_html, PURPLE_MESSAGE_SEND, mtime);
						g_strfreev(chatusers);
					} else {
						serv_got_im(gc, sender, body_html, PURPLE_MESSAGE_RECV, mtime);
					}
				}/* else if (strcmp(type, "AUTHREQUEST") == 0 && strcmp(sender, my_username) != 0)
				{
					purple_debug_info("User %s requested alternate authorisation\n", sender);
					purple_account_request_authorization(this_account, sender, NULL, skype_get_user_info(sender, "FULLNAME"),
												body, (purple_find_buddy(this_account, sender) != NULL),
												skype_auth_allow, skype_auth_deny, (gpointer)g_strdup(sender));
				}*/

				skype_send_message("SET MESSAGE %s SEEN", msg_num);
			}
		} else if (strcmp(string_parts[3], "SENT") == 0)
		{
			/* mark it as seen, to remove notification from skype ui */
			
			/* dont async this -> infinite loop */
			skype_send_message("SET MESSAGE %s SEEN", string_parts[1]);
		}
	} else if (strcmp(command, "CHATMESSAGE") == 0)
	{
		if (strcmp(string_parts[3], "RECEIVED") == 0)
		{
			msg_num = string_parts[1];
			temp = skype_send_message("GET CHATMESSAGE %s TYPE", msg_num);
			type = g_strdup(&temp[18+strlen(msg_num)]);
			g_free(temp);
			temp = skype_send_message("GET CHATMESSAGE %s CHATNAME", msg_num);
			chatname = g_strdup(&temp[22+strlen(msg_num)]);
			g_free(temp);
			
			glist_temp = g_list_find_custom(purple_get_conversations(), chatname, (GCompareFunc)skype_find_chat);
			if (glist_temp == NULL || glist_temp->data == NULL)
			{
				temp = skype_send_message("GET CHAT %s STATUS", chatname);
				chat_type = g_strdup(&temp[13+strlen(chatname)]);
				g_free(temp);
				if (strcmp(chat_type, "DIALOG") == 0 || strcmp(chat_type, "LEGACY_DIALOG") == 0)
				{
					temp = skype_send_message("GET CHAT %s MEMBERS", chatname);
					body = g_strdup(&temp[14+strlen(chatname)]);
					g_free(temp);
					chatusers = g_strsplit(body, " ", 0);
					if (strcmp(chatusers[0], my_username) == 0)
						sender = g_strdup(chatusers[1]);
					else
						sender = g_strdup(chatusers[0]);
					g_strfreev(chatusers);
					g_free(body);
					////if they have an IM window open, assign it the chatname
					//
					////if they dont have an IM window open, open one, then look again
					//serv_got_im(gc, sender, " ", PURPLE_MESSAGE_SYSTEM & PURPLE_MESSAGE_NO_LOG & PURPLE_MESSAGE_NOTIFY, 1);
					conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, sender, this_account);
					//TODO need to be able to fix for adium which doesn't create conversations
					//if (conv == NULL)
					//	conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, this_account, sender);
				} else {
					conv = serv_got_joined_chat(gc, chat_count++, chatname);
					//conv = purple_conversation_new(PURPLE_CONV_TYPE_CHAT, this_account, chatname);
					temp = skype_send_message("GET CHAT %s MEMBERS", chatname);
					body = g_strdup(&temp[14+strlen(chatname)]);
					g_free(temp);
					chatusers = g_strsplit(body, " ", 0);
					for (i=0; chatusers[i]; i++)
						purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), chatusers[i], NULL, PURPLE_CBFLAGS_NONE, FALSE);
					g_strfreev(chatusers);
					g_free(body);
					temp = skype_send_message("GET CHAT %s TOPIC", chatname);
					body = g_strdup(&temp[12+strlen(chatname)]);
					g_free(temp);
					purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), this_account->username, body);
					purple_debug_info("skype", "set topic to: %s\n", body);
				}
				purple_conversation_set_data(conv, "chat_id", chatname);
				//g_hash_table_insert(conv->data, "chat_id", chatname);
				//conv = purple_conversation_new(PURPLE_CONV_TYPE_ANY, this_account, chatname);
			} else {
				conv = glist_temp->data;
			}
			//Types of chat message are:
			//    SETTOPIC - change of chat topic
			//    SAID - IM
			//    ADDEDMEMBERS - invited someone to chat
			//    SAWMEMBERS - chat participant has seen other members
			//    CREATEDCHATWITH - chat to multiple people is created
			//    LEFT - someone left chat
			if (strcmp(type, "SETTOPIC") == 0 && conv && conv->type == PURPLE_CONV_TYPE_CHAT)
			{
				temp = skype_send_message("GET CHATMESSAGE %s BODY", msg_num);
				body = g_strdup(&temp[18+strlen(msg_num)]);
				g_free(temp);
				purple_debug_info("skype", "Topic changed: %s\n", body);
				purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), NULL, body);
				temp = skype_send_message("GET CHATMESSAGE %s FROM_HANDLE", msg_num);
				sender = g_strdup(&temp[25+strlen(msg_num)]);
				g_free(temp);
				serv_got_chat_in(gc, purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)), sender, PURPLE_MESSAGE_SYSTEM, skype_strdup_withhtml(g_strconcat(sender, _(" changed the topic to "), body, NULL)), time(NULL));
			} else if (strcmp(type, "SAID") == 0 ||
						strcmp(type, "TEXT") == 0)
			{
				temp = skype_send_message("GET CHATMESSAGE %s BODY", msg_num);
				body = g_strdup(&temp[18+strlen(msg_num)]);
				g_free(temp);
				//purple_debug_info("skype", "Message received: %s\n", body);
				temp = skype_send_message("GET CHATMESSAGE %s FROM_HANDLE", msg_num);
				sender = g_strdup(&temp[25+strlen(msg_num)]);
				g_free(temp);
				temp = skype_send_message("GET CHATMESSAGE %s TIMESTAMP", msg_num);
				mtime = atoi(&temp[23+strlen(msg_num)]);
				g_free(temp);

				/* Escape the body to HTML */
				body_html = skype_strdup_withhtml(body);
				g_free(body);
				if (conv && conv->type == PURPLE_CONV_TYPE_CHAT)
					serv_got_chat_in(gc, purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)), sender, PURPLE_MESSAGE_RECV, body_html, mtime);
				else
				{
					if (strcmp(sender, my_username) == 0)
					{
						g_free(sender);
						temp = skype_send_message("GET CHAT %s MEMBERS", chatname);
						chatusers = g_strsplit(&temp[14+strlen(chatname)], " ", 0);
						if (strcmp(chatusers[0], my_username) == 0)
							sender = g_strdup(chatusers[1]);
						else
							sender = g_strdup(chatusers[0]);
						g_strfreev(chatusers);
						g_free(temp);
						serv_got_im(gc, sender, body_html, PURPLE_MESSAGE_SEND, mtime);
					} else {
						serv_got_im(gc, sender, body_html, PURPLE_MESSAGE_RECV, mtime);
					}
				}
			} else if (strcmp(type, "ADDEDMEMBERS") == 0 && conv && conv->type == PURPLE_CONV_TYPE_CHAT)
			{
				temp = skype_send_message("GET CHATMESSAGE %s USERS", msg_num);
				body = g_strdup(&temp[19+strlen(msg_num)]);
				g_free(temp);
				purple_debug_info("skype", "Friends added: %s\n", body);
				chatusers = g_strsplit(body, " ", 0);
				for (i=0; chatusers[i]; i++)
					purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), chatusers[i], NULL, PURPLE_CBFLAGS_NONE, FALSE);
				g_strfreev(chatusers);
				g_free(body);
			} else if (strcmp(type, "LEFT") == 0 && conv && conv->type == PURPLE_CONV_TYPE_CHAT)
			{
				temp = skype_send_message("GET CHATMESSAGE %s USERS", msg_num);
				body = g_strdup(&temp[19+strlen(msg_num)]);
				g_free(temp);
				purple_debug_info("skype", "Friends left: %s\n", body);
				temp = skype_send_message("GET CHATMESSAGE %s LEAVEREASON", msg_num);
				purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv), body, g_strdup(&temp[25+strlen(msg_num)]));
			}
			/* dont async this -> infinite loop */
			skype_send_message("SET CHATMESSAGE %s SEEN", msg_num);
		}
	} else if (strcmp(command, "FILETRANSFER") == 0)
	{
		//lookup current file transfers to see if there's already one there
		glist_temp = g_list_find_custom(purple_xfers_get_all(),
										string_parts[1],
										(GCompareFunc)skype_find_filetransfer);
		if (glist_temp == NULL && strcmp(string_parts[2], "TYPE") == 0)
		{
			temp = skype_send_message("GET FILETRANSFER %s PARTNER_HANDLE", string_parts[1]);
			sender = g_strdup(&temp[29+strlen(string_parts[1])]);
			g_free(temp);
			if (strcmp(string_parts[2], "INCOMING") == 0)
			{
				transfer = purple_xfer_new(this_account, PURPLE_XFER_RECEIVE, sender);
			} else {
				transfer = purple_xfer_new(this_account, PURPLE_XFER_SEND, sender);
			}
			transfer->data = g_strdup(string_parts[1]);
			purple_xfer_set_init_fnc(transfer, skype_accept_transfer);
			purple_xfer_set_request_denied_fnc(transfer, skype_decline_transfer);
			temp = skype_send_message("GET FILETRANSFER %s FILENAME", string_parts[1]);
			purple_debug_info("skype", "Filename: '%s'\n", &temp[23+strlen(string_parts[1])]);
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
			/*if (strcmp(string_parts[2], "TYPE") == 0)
			{
				if (strcmp(string_parts[3], "INCOMING") == 0)
				{
					transfer->type = PURPLE_XFER_RECEIVE;
				} else {
					transfer->type = PURPLE_XFER_SEND;
				}
			} else if (strcmp(string_parts[2], "PARTNER_HANDLE") == 0)
			{
				transfer->who = g_strdup(string_parts[3]);
			} else*/ if (strcmp(string_parts[2], "FILENAME") == 0)
			{
				purple_xfer_set_filename(transfer, string_parts[3]);
			} else if (strcmp(string_parts[2], "FILEPATH") == 0)
			{
				if (strlen(string_parts[3]))
					purple_xfer_set_local_filename(transfer, string_parts[3]);
			} else if (strcmp(string_parts[2], "STATUS") == 0)
			{
				if (strcmp(string_parts[3], "NEW") == 0 ||
					strcmp(string_parts[3], "WAITING_FOR_ACCEPT") == 0)
				{
					//Skype API doesn't let us accept transfers
					//purple_xfer_request(transfer);
					if (purple_xfer_get_type(transfer) == PURPLE_XFER_RECEIVE)
					{
#						ifndef __APPLE__
							skype_send_message("OPEN FILETRANSFER");
#						else
							purple_notify_info(this_account, "Incoming File", g_strconcat("User ", purple_xfer_get_remote_user(transfer), " wishes to send you a file.  Please open Skype to accept this file.", NULL), NULL);
#						endif
						purple_xfer_conversation_write(transfer, g_strconcat(purple_xfer_get_remote_user(transfer), " is sending a file to users of this chat.", NULL), FALSE);
					}
					purple_xfer_set_status(transfer, PURPLE_XFER_STATUS_NOT_STARTED);
				} else if (strcmp(string_parts[3], "COMPLETED") == 0)
				{
					purple_xfer_set_completed(transfer, TRUE);
				} else if (strcmp(string_parts[3], "CONNECTING") == 0 ||
							strcmp(string_parts[3], "TRANSFERRING") == 0 ||
							strcmp(string_parts[3], "TRANSFERRING_OVER_RELAY") == 0)
				{
					purple_xfer_set_status(transfer, PURPLE_XFER_STATUS_STARTED);
					transfer->start_time = time(NULL);
				} else if (strcmp(string_parts[3], "CANCELLED") == 0)
				{
					//transfer->end_time = time(NULL);
					//transfer->bytes_remaining = 0;
					//purple_xfer_set_status(transfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
					purple_xfer_cancel_local(transfer);
				}/* else if (strcmp(string_parts[3], "FAILED") == 0)
				{
					//transfer->end_time = time(NULL);
					//purple_xfer_set_status(transfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
					purple_xfer_cancel_remote(transfer);
				}*/
				purple_xfer_update_progress(transfer);
			} else if (strcmp(string_parts[2], "STARTTIME") == 0)
			{
				transfer->start_time = atol(string_parts[3]);
				purple_xfer_update_progress(transfer);
			/*} else if (strcmp(string_parts[2], "FINISHTIME") == 0)
			{
				if (strcmp(string_parts[3], "0") != 0)
					transfer->end_time = atol(string_parts[3]);
				purple_xfer_update_progress(transfer);*/
			} else if (strcmp(string_parts[2], "BYTESTRANSFERRED") == 0)
			{
				purple_xfer_set_bytes_sent(transfer, atol(string_parts[3]));
				purple_xfer_update_progress(transfer);
			} else if (strcmp(string_parts[2], "FILESIZE") == 0)
			{
				purple_xfer_set_size(transfer, atol(string_parts[3]));
			} else if (strcmp(string_parts[2], "FAILUREREASON") == 0 &&
						strcmp(string_parts[3], "UNKNOWN") != 0)
			{
				temp = NULL;
				if (strcmp(string_parts[3], "SENDER_NOT_AUTHORIZED") == 0)
				{
					temp = g_strdup(_("Not authorized"));
				} else if (strcmp(string_parts[3], "REMOTELY_CANCELLED") == 0)
				{
					purple_xfer_cancel_remote(transfer);
					purple_xfer_update_progress(transfer);
				} else if (strcmp(string_parts[3], "FAILED_READ") == 0)
				{
					temp = g_strdup(_("Read error on local machine"));
				} else if (strcmp(string_parts[3], "FAILED_REMOTE_READ") == 0)
				{
					temp = g_strdup(_("Read error on remote machine"));
				} else if (strcmp(string_parts[3], "FAILED_WRITE") == 0)
				{
					temp = g_strdup(_("Write error on local machine"));
				} else if (strcmp(string_parts[3], "FAILED_REMOTE_WRITE") == 0)
				{
					temp = g_strdup(_("Write error on remote machine"));
				} else if (strcmp(string_parts[3], "REMOTE_DOES_NOT_SUPPORT_FT") == 0)
				{
					temp = g_strdup(_("Receiver does not support file transfers"));
				} else if (strcmp(string_parts[3], "REMOTE_OFFLINE_FOR_TOO_LONG") == 0)
				{
					temp = g_strdup(_("Recipient not available"));
				}
				if (temp && strlen(temp))
				{
					purple_xfer_error(transfer->type, this_account, transfer->who, temp);
					g_free(temp);
				}
			}
		}
	} else if (strcmp(command, "WINDOWSTATE") == 0)
	{
		if (strcmp(string_parts[1], "HIDDEN") == 0)
		{
			skype_send_message("SET SILENT_MODE ON");
		}
#ifdef USE_FARSIGHT
	} else if (strcmp(command, "CALL") == 0)
	{
		if (strcmp(string_parts[2], "STATUS") == 0)
		{ 
			if (strcmp(string_parts[3], "RINGING") == 0)
			{
				skype_handle_incoming_call(gc, string_parts[1]);
			} else if (strcmp(string_parts[3], "FINISHED") == 0 ||
						strcmp(string_parts[3], "CANCELLED") == 0 ||
						strcmp(string_parts[3], "FAILED") == 0)
			{
				skype_handle_call_got_ended(string_parts[1]);
			}
		}
#else
	} else if (strcmp(command, "CALL") == 0)
	{
		if (strcmp(string_parts[2], "STATUS") == 0 &&
			strcmp(string_parts[3], "RINGING") == 0)
		{
			temp = skype_send_message("GET CALL %s TYPE", string_parts[1]);
			type = g_new0(gchar, 9);
			sscanf(temp, "CALL %*s TYPE %[^_]", type);
			g_free(temp);
			temp = skype_send_message("GET CALL %s PARTNER_HANDLE", string_parts[1]);
			sender = g_strdup(&temp[21+strlen(string_parts[1])]);
			g_free(temp);
			if (strcmp(type, "INCOMING") == 0)
			{
				purple_request_action(gc, _("Incoming Call"), g_strconcat(sender, " is calling you", NULL), "Do you want to accept their call?",
								0, this_account, sender, NULL, g_strdup(string_parts[1]), 2, _("Accept"), 
								G_CALLBACK(skype_call_accept_cb), _("Reject"), G_CALLBACK(skype_call_reject_cb));
			}
			g_free(sender);
			g_free(type);
		}
#endif	
	}
	if (string_parts)
	{
		g_strfreev(string_parts);
	}
	return FALSE;
}

void
skype_call_accept_cb(gchar *call)
{
	skype_send_message("ALTER CALL %s ANSWER", call);
	g_free(call);
}

void
skype_call_reject_cb(gchar *call)
{
	skype_send_message("ALTER CALL %s HANGUP", call);
	g_free(call);
}

void
skype_auth_allow(gpointer sender)
{
	skype_send_message("SET USER %s ISAUTHORIZED TRUE", sender);
}

void
skype_auth_deny(gpointer sender)
{
	skype_send_message("SET USER %s ISAUTHORIZED FALSE", sender);
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

gint
skype_find_chat(PurpleConversation *conv, char *chat_id)
{
	char *lookup;
	if (chat_id == NULL || conv == NULL || conv->data == NULL)
		return -1;
	//lookup = g_hash_table_lookup(conv->data, "chat_id");
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
