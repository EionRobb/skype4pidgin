/*
 *  libskypekit.cpp
 *  skypekit
 *
 *  Created by MyMacSpace on 19/04/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include "libskypekit.h"

Contact::Ref find_contact(Skype *skype, const char *who)
{
	SEString skypename = who;
	ContactGroup::Ref cg;
	Contact::Refs contacts;
	if (!skype->GetHardwiredContactGroup(ContactGroup::ALL_KNOWN_CONTACTS, cg) || !cg->GetContacts(contacts)) {
		//Unable to get contact list
		return Contact::Ref(0);
	}
	
	Sid::String identity; 
	for (uint i = 0; i < contacts.size(); i++) {
		contacts[i]->GetIdentity(identity);
		if (contacts[i] && identity == skypename) {
			return contacts[i];
		}
	}
	
	return Contact::Ref(0);
}

ContactGroup::Ref find_group(Skype *skype, Sid::String& groupname)
{
	ContactGroup::Refs cgs;
	ContactGroup::Ref cg;
	if (!skype->GetCustomContactGroups(cgs))
		return ContactGroup::Ref(0);
	
	while((cg = cgs.peek()))
	{
		SEString displayname;
		cg->GetPropGivenDisplayname(displayname);
		if (displayname.equals(groupname))
			return cg;
	}
	
	return ContactGroup::Ref(0);
}

Conversation::Ref find_conversation(Skype *skype, const char *who)
{
	SEStringList participants;
	Conversation::Ref conv;
	Sid::String skypename = who;

	participants.append(skypename);
	skype->GetConversationByParticipants(participants, conv, true, false);
	
	return conv;
}

const char *skype_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "skype";
}

GList *skype_status_types(PurpleAccount *acct)
{
	GList *types;
	PurpleStatusType *status;
	
	purple_debug_info("skype", "returning status types\n");
	
	types = NULL;
	
    /* Statuses are almost all the same. Define a macro to reduce code repetition. */
#define _SKYPE_ADD_NEW_STATUS(prim,id,name) status =            \
	purple_status_type_new_with_attrs(                      \
	prim,    /* PurpleStatusPrimitive */                    \
	id,      /* id  */                                      \
	_(name), /* name */                                     \
	TRUE,    /* savable */                                  \
	TRUE,    /* user_settable */                            \
	FALSE,   /* not independent */                          \
	\
	/* Attributes - each status can have a message. */      \
	"message",                                              \
	_("Mood"),                                              \
	purple_value_new(PURPLE_TYPE_STRING),                   \
	NULL);                                                  \
	\
	\
	types = g_list_append(types, status)
	
	
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_AVAILABLE, "ONLINE", "Online");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_AVAILABLE, "SKYPEME", "SkypeMe");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_AWAY, "AWAY", "Away");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_EXTENDED_AWAY, "NA", "Not Available");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_UNAVAILABLE, "DND", "Do Not Disturb");
	_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_INVISIBLE, "INVISIBLE", "Invisible");
	//_SKYPE_ADD_NEW_STATUS(PURPLE_STATUS_OFFLINE, "Offline");
	
	//User status could be 'logged out' - make not settable
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, "LOGGEDOUT", _("Logged out"), FALSE, FALSE, FALSE);
	types = g_list_append(types, status);
	
	//User could be a SkypeOut contact
	if (purple_account_get_bool(acct, "skypeout_online", TRUE))
	{
		status = purple_status_type_new_full(PURPLE_STATUS_MOBILE, "SKYPEOUT", _("SkypeOut"), FALSE, FALSE, FALSE);
	} else {
		status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, "SKYPEOUT", _("SkypeOut"), FALSE, FALSE, FALSE);
	}
	types = g_list_append(types, status);
	
	//Offline people shouldn't have status messages
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, "OFFLINE", _("Offline"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	return types;
}

void skype_login(PurpleAccount *acct)
{
	PurpleConnection *pc;
	SEString AppToken = SKYPE_APP_TOKEN;
	MyAccount::Ref A;
	MySkype *skype = 0;
	SETCPTransport *transport = 0;
	SEString accountName = acct->username;
	SEString accountPW = acct->password;
	
	pc = purple_account_get_connection(acct);
	pc->proto_data = NULL;
	pc->flags = (PurpleConnectionFlags) (PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_NO_BGCOLOR);
	
	transport = new SETCPTransport(purple_account_get_string(acct, "host", "127.0.0.1"), 
		purple_account_get_int(acct, "port", 8963));
	skype = new MySkype(transport);
	pc->proto_data = skype;
	
	purple_debug_info("skypekit", "Pre init()\n");
	if (!skype->init(1, 1))
	{
		purple_connection_error(pc, "Could not connect");
		return;
	}
	purple_debug_info("skypekit", "Pre start()\n");
	skype->start();
	purple_debug_info("skypekit", "Post start()\n");
	skype->SetApplicationToken(AppToken);
	purple_debug_info("skypekit", "Set token to %s\n", SKYPE_APP_TOKEN);
	
	skype->pa = acct;
	skype->pc = pc;
	
	if (skype->GetAccount(accountName, A))
	{
		purple_debug_info("skypekit", "Got account\n");
		if (!A->LoginWithPassword(accountPW, false, true))
		{
			purple_connection_error(pc, "Invalid username or password2");
			return;
		}
	} else {
		purple_connection_error(pc, "Invalid username or password");
		return;
	}

	A->SetAvailability(Contact::ONLINE);

}

void skype_close(PurpleConnection *pc)
{
	Skype *skype = (Skype *) pc->proto_data;
	PurpleAccount *account = pc->account;
	MyAccount::Ref A;
	SEString accountName = account->username;
	
	if (!pc || !pc->proto_data)
		return;

	purple_debug_info("skypekit", "Logging out\n");
	if(skype->GetAccount(accountName, A))
	{
		purple_debug_info("skypekit", "...\n");
		A->Logout(true);
	}
	purple_debug_info("skypekit", "Logged out\n");
	
	skype->stop();
	purple_debug_info("skypekit", "Stopped\n");
	skype->cleanup();
	purple_debug_info("skypekit", "Clean\n");
	
	delete skype;
}

int skype_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	Skype *skype = (Skype *)gc->proto_data;
	Message::Ref msg;
	
	ConversationRef conv = find_conversation(skype, who);
	
	if (conv->PostText(message, msg, true))
	{
		return strlen(message);
	} else {
		return -1;
	}
}

int
skype_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
	MySkype *skype = (MySkype *)gc->proto_data;
	Conversation *conv = skype->newConversation(id);
	Message::Ref msg;
	
	if (conv->PostText(message, msg, true))
	{
		serv_got_chat_in(gc, id, purple_account_get_username(purple_connection_get_account(gc)), PURPLE_MESSAGE_SEND,
						message, time(NULL));

		return 1;
	} else {
		return -1;
	}
}

void
skype_set_chat_topic(PurpleConnection *gc, int id, const char *topic)
{
	MySkype *skype = (MySkype *)gc->proto_data;
	Conversation *conv = skype->newConversation(id);
	
	conv->SetTopic(topic);
}

void
skype_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	MySkype *skype = (MySkype *)pc->proto_data;
	PurpleStatusType *type;
	const char *message;
	const char *statusid;
	MyAccount::Ref A;
	SEString accountName = account->username;
	
	if (!skype->GetAccount(accountName, A))
		return;
	
	type = purple_status_get_type(status);
	statusid = purple_status_type_get_id(type);
	if (g_str_equal(statusid, "ONLINE"))
	{
		A->SetAvailability(Contact::ONLINE);
	} else if (g_str_equal(statusid, "SKYPEME"))
	{
		A->SetAvailability(Contact::SKYPE_ME);
	} else if (g_str_equal(statusid, "AWAY"))
	{
		A->SetAvailability(Contact::AWAY);
	} else if (g_str_equal(statusid, "NA"))
	{
		A->SetAvailability(Contact::NOT_AVAILABLE);
	} else if (g_str_equal(statusid, "DND"))
	{
		A->SetAvailability(Contact::DO_NOT_DISTURB);
	} else if (g_str_equal(statusid, "INVISIBLE"))
	{
		A->SetAvailability(Contact::INVISIBLE);
	} else if (g_str_equal(statusid, "OFFLINE"))
	{
		A->SetAvailability(Contact::OFFLINE);
	}
	
	message = purple_status_get_attr_string(status, "message");
	if (message == NULL)
		message = "";
	else
		message = purple_markup_strip_html(message);
	
	A->SetStrProperty(Contact::P_MOOD_TEXT, message);
}

unsigned int
skype_send_typing(PurpleConnection *gc, const gchar *who, PurpleTypingState state)
{
	Skype *skype = (Skype *)gc->proto_data;
	Message::Ref msg;
	
	//Don't send typing notifications to self
	if (g_str_equal(who, gc->account->username))
		return 999;
	
	ConversationRef conv = find_conversation(skype, who);
	
	switch(state)
	{
		case PURPLE_NOT_TYPING:
			conv->SetMyTextStatusTo(Participant::TEXT_NA);
			break;
		case PURPLE_TYPING:
			conv->SetMyTextStatusTo(Participant::WRITING);
			break;
		case PURPLE_TYPED:
			conv->SetMyTextStatusTo(Participant::READING);
			break;
		default:
			break;
	}
	
	return 999;
}

const char *
skype_normalize(const PurpleAccount *acct, const char *who)
{
	static gchar *last_normalize = NULL;
	g_free(last_normalize);
	last_normalize = g_utf8_strdown(who, -1);
	return last_normalize;
}

void skype_add_deny(PurpleConnection *gc, const char *who)
{
	Skype *skype = (Skype *)gc->proto_data;
	Contact::Ref contact = find_contact(skype, who);
	
	if (contact)
	{
		contact->SetBlocked(true);
	}
}
void skype_rem_deny(PurpleConnection *gc, const char *who)
{
	Skype *skype = (Skype *)gc->proto_data;
	Contact::Ref contact = find_contact(skype, who);
	
	if (contact)
	{
		contact->SetBlocked(false);
	}
}
void skype_add_permit(PurpleConnection *gc, const char *who)
{
	Skype *skype = (Skype *)gc->proto_data;
	Contact::Ref contact = find_contact(skype, who);
	
	if (contact)
	{
		contact->SetBuddyStatus(true);
	}
}
void
skype_rem_permit(PurpleConnection *gc, const char *who)
{
	Skype *skype = (Skype *)gc->proto_data;
	Contact::Ref contact = find_contact(skype, who);
	
	if (contact)
	{
		contact->SetBuddyStatus(false);
	}
}

char *
skype_status_text(PurpleBuddy *buddy)
{
	PurpleAccount *account = purple_buddy_get_account(buddy);
	PurpleConnection *pc = purple_account_get_connection(account);
	Skype *skype = (Skype *)pc->proto_data;
	SEString mood;
	
	Contact::Ref contact = find_contact(skype, buddy->name);
	
	if (contact)
	{
		mood = contact->GetProp(Contact::P_MOOD_TEXT);
	}
	
	if (mood.isEmpty())
	{
		return NULL;
	}
	
	return g_strdup(mood);
}

void
skype_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *userinfo, gboolean full)
{
	PurplePresence *presence;
	PurpleStatus *status;
	const gchar *text;

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);
	purple_notify_user_info_add_pair(userinfo, _("Status"), purple_status_get_name(status));
	
	text = purple_status_get_attr_string(status, "message");
	if (text)
		purple_notify_user_info_add_pair(userinfo, _("Mood"), text);
}

void
skype_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	Skype *skype = (Skype *)pc->proto_data;
	bool Found = false;
	Contact::Ref NewContact;
	SEString name = buddy->name;
	SEString alias = buddy->alias;
	ContactGroup::Ref NewGroup;
	SEString groupname = group->name;
	
	Found = skype->GetContact(name, NewContact);
	if (!Found)
		return;
	
	NewContact->GiveDisplayName(alias);
	NewContact->SendAuthRequest("");
	NewContact->SetBuddyStatus(true);
	
	NewGroup = find_group(skype, groupname);
	if (!NewGroup)
	{
		skype->CreateCustomContactGroup(NewGroup);
		NewGroup->GiveDisplayName(groupname);
	}
	NewGroup->AddContact(NewContact);

}

void
skype_remove_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	Skype *skype = (Skype *)pc->proto_data;
	bool Found = false;
	Contact::Ref RemovableContact;
	SEString name = buddy->name;
	SEString alias = buddy->alias;
	ContactGroup::Ref NewGroup;
	SEString groupname = group->name;
	
	Found = skype->GetContact(name, RemovableContact);
	
	NewGroup = find_group(skype, groupname);
	if (NewGroup)
		NewGroup->RemoveContact(RemovableContact);
}

const char *
skype_list_emblem(PurpleBuddy *buddy)
{
	PurpleConnection *pc = purple_account_get_connection(buddy->account);
	Skype *skype = (Skype *)pc->proto_data;
	Contact::Ref contact = find_contact(skype, buddy->name);
	Contact::AVAILABILITY avail;
	bool hasCap = false;
	
	if (contact)
	{
		contact->GetPropAvailability(avail);
		switch(avail)
		{
			case Contact::ONLINE_FROM_MOBILE:
			case Contact::AWAY_FROM_MOBILE:
			case Contact::NOT_AVAILABLE_FROM_MOBILE:
			case Contact::DO_NOT_DISTURB_FROM_MOBILE:
			case Contact::SKYPE_ME_FROM_MOBILE:
				return "mobile";
				break;
			default:
				break;
		}
		if (contact->HasCapability(Contact::CAPABILITY_MOBILE_DEVICE, hasCap)
			&& hasCap == true)
			return "mobile";
		
		if (contact->HasCapability(Contact::CAPABILITY_VIDEO, hasCap) 
			&& hasCap == true)
			return "video";
	}
	
	return NULL;
}

gboolean
skype_offline_msg(const PurpleBuddy *buddy)
{
	return TRUE;
}

// Since this function isn't public, and we need it to be, redefine it here
static void
purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatusType status)
{
	g_return_if_fail(xfer != NULL);
	
	if (xfer->status == status)
		return;

	xfer->status = status;

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
}

gboolean
skype_can_receive_file(PurpleConnection *pc, const char *who)
{
	return TRUE;
}

void
skype_xfer_cancel_send(PurpleXfer *xfer)
{
	purple_debug_info("skypekit", "xfer_cancel_send\n");
	Transfer *transfer = (Transfer *) xfer->data;
	if (transfer != NULL)
	{
		transfer->Cancel();
	}
}

void
skype_xfer_cancel_recv(PurpleXfer *xfer)
{
	purple_debug_info("skypekit", "xfer_cancel_recv\n");
	Transfer *transfer = (Transfer *) xfer->data;
	if (transfer != NULL)
	{
		transfer->Cancel();
	}
}

void
skype_xfer_init(PurpleXfer *xfer)
{
	purple_debug_info("skypekit", "xfer_init\n");
	Skype *skype = (Skype *)xfer->account->gc->proto_data;
	SEFilenameList filenames;
	Message::Ref msg;
	TRANSFER_SENDFILE_ERROR error;
	SEFilename error_filename;
	SEString body;
	
	if (xfer == NULL)
		return;
	
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_NOT_STARTED);
	
	if (xfer->type == PURPLE_XFER_SEND)
	{
		ConversationRef conv = find_conversation(skype, xfer->who);
		
		filenames.append(xfer->local_filename);	
		body += xfer->message;
		if (!conv->PostFiles(filenames, body, error, error_filename) || error)
		{
			purple_debug_error("skypekit", "Failed to post %s (from %s) because %d\n", (const char *)error_filename, (const char *)body, error);
			purple_xfer_error(xfer->type, xfer->account, xfer->who, "Error sending file");
			purple_xfer_cancel_local(xfer);
			return;
		} else {
			purple_debug_info("skypekit", "Send file %s\n", xfer->local_filename);
			purple_xfer_set_cancel_send_fnc(xfer, skype_xfer_cancel_send);
		}
	} else if (xfer->type == PURPLE_XFER_RECEIVE)
	{
		purple_debug_info("skypekit", "incoming file\n");
		bool result;
		//transfer->Accept("path", result);
		//if (!result) {
			//accept failed
		//}
		Transfer *transfer = (Transfer *) xfer->data;
		if (transfer != NULL)
		{
			transfer->Accept(purple_xfer_get_local_filename(xfer), result);
			if (!result)
			{
				purple_debug_error("skypekit", "accept failed\n");
			} else {
				purple_xfer_set_cancel_recv_fnc(xfer, skype_xfer_cancel_recv);
			}
		}
	}
}

void
skype_xfer_start(PurpleXfer *xfer)
{
	purple_debug_info("skypekit", "xfer_start\n");
}

void
skype_xfer_end(PurpleXfer *xfer)
{
	purple_debug_info("skypekit", "xfer_end\n");
}

//called when the incoming file request is denied
void
skype_xfer_request_denied(PurpleXfer *xfer)
{
	purple_debug_info("skypekit", "xfer_request_denied\n");
	Transfer *transfer = (Transfer *) xfer->data;
	if (transfer != NULL)
	{
		transfer->Cancel();
	}
}

PurpleXfer *
skype_xfer_new(PurpleConnection *pc, const char *who)
{
	purple_debug_info("skypekit", "xfer_new\n");
	PurpleXfer *xfer = NULL;
	
	xfer = purple_xfer_new(pc->account, PURPLE_XFER_SEND, who);
	
	purple_xfer_set_init_fnc(xfer, skype_xfer_init);
	//purple_xfer_set_start_fnc(xfer, skype_xfer_start);
	//purple_xfer_set_end_fnc(xfer, skype_xfer_end);
	purple_xfer_set_cancel_send_fnc(xfer, skype_xfer_cancel_send);
	//purple_xfer_set_cancel_recv_fnc(xfer, skype_xfer_cancel_recv);
	
	return xfer;
}

void
skype_send_file(PurpleConnection *pc, const char *who, const char *filename)
{
	purple_debug_info("skypekit", "send_file\n");
	PurpleXfer *xfer = skype_xfer_new(pc, who);
	
	if (filename)
	{
		purple_xfer_request_accepted(xfer, filename);
	} else {
		purple_xfer_request(xfer);
	}
}

void 
skype_get_info(PurpleConnection *pc, const gchar *username)
{
	PurpleNotifyUserInfo *user_info;
	Skype *skype = (Skype *) pc->proto_data;
	MyContact::Ref contact = find_contact(skype, username);
	
	SEIntList Keys;
	SEIntDict Values;
	
	Keys.append(Contact::P_SKYPENAME);
	Keys.append(Contact::P_FULLNAME);
	Keys.append(Contact::P_MOOD_TEXT);
	Keys.append(Contact::P_BIRTHDAY);
	Keys.append(Contact::P_GENDER);
	Keys.append(Contact::P_LANGUAGES);
	Keys.append(Contact::P_COUNTRY);
	Keys.append(Contact::P_GIVEN_AUTHLEVEL);
	Keys.append(Contact::P_TIMEZONE);
	Keys.append(Contact::P_NROF_AUTHED_BUDDIES);
	Keys.append(Contact::P_ABOUT);
	Values = contact->GetProps(Keys);
	
	user_info = purple_notify_user_info_new();
	
	purple_notify_user_info_add_section_header(user_info, _("Contact Info"));
	purple_notify_user_info_add_pair(user_info, _("Skype Name"), Values.find(Contact::P_SKYPENAME));
	purple_notify_user_info_add_pair(user_info, _("Full Name"), Values.find(Contact::P_FULLNAME));
	purple_notify_user_info_add_pair(user_info, _("Mood Text"), Values.find(Contact::P_MOOD_TEXT));
	
	purple_notify_user_info_add_section_break(user_info);
	
	purple_notify_user_info_add_section_header(user_info, _("Personal Information"));
	struct tm birthday_time;
	purple_str_to_time(Values.find(Contact::P_BIRTHDAY), TRUE, &birthday_time, NULL, NULL);
	purple_notify_user_info_add_pair(user_info, _("Birthday"), purple_date_format_short(&birthday_time));
	purple_notify_user_info_add_pair(user_info, _("Gender"), Values.find(Contact::P_GENDER).toUInt() == 1 ? _("Male") : _("Female"));
	purple_notify_user_info_add_pair(user_info, _("Preferred Language"), Values.find(Contact::P_LANGUAGES));
	purple_notify_user_info_add_pair(user_info, _("Country"), Values.find(Contact::P_COUNTRY));
	purple_notify_user_info_add_pair(user_info, _("Authorization Granted"), Values.find(Contact::P_GIVEN_AUTHLEVEL).toUInt() == Contact::AUTHORIZED_BY_ME ? _("Yes") : _("No"));
	purple_notify_user_info_add_pair(user_info, _("Blocked"), Values.find(Contact::P_GIVEN_AUTHLEVEL).toUInt() == Contact::BLOCKED_BY_ME ? _("Yes") : _("No"));
	
	gchar *temp;
	purple_notify_user_info_add_pair(user_info, _("Timezone"), temp = g_strdup_printf("UMT %+.1f", ((double)Values.find(Contact::P_TIMEZONE).toUInt()) / 3600 - 24));
	g_free(temp);
	
	purple_notify_user_info_add_pair(user_info, _("Number of buddies"), Values.find(Contact::P_NROF_AUTHED_BUDDIES));
	
	purple_notify_user_info_add_section_break(user_info);
	
	purple_notify_user_info_add_pair(user_info, NULL, Values.find(Contact::P_ABOUT));

	purple_notify_userinfo(pc, username, user_info, NULL, NULL);
	purple_notify_user_info_destroy(user_info);
}

void
skype_alias_buddy(PurpleConnection *pc, const char *who, const char *alias)
{
	Skype *skype = (Skype *)pc->proto_data;
	SEString displayname = alias;
	Contact::Ref contact = find_contact(skype, who);
	
	contact->GiveDisplayName(displayname);
}

void
skype_set_buddy_icon(PurpleConnection *pc, PurpleStoredImage *img)
{
	Skype *skype = (Skype *)pc->proto_data;
	MyAccount::Ref A;
	SEString accountName = pc->account->username;
	SEBinary avatarImg;

	avatarImg.set(purple_imgstore_get_data(img), purple_imgstore_get_size(img));
	
	if (!skype->GetAccount(accountName, A))
		return;
	
	A->SetBinProperty(Account::P_AVATAR_IMAGE, avatarImg);
}

static const char *
skype_contact_status_to_id(int availability)
{
	const char *status;
	switch(availability)
	{
		case Contact::ONLINE:
		case Contact::ONLINE_FROM_MOBILE:
			status = "ONLINE";
			break;
		case Contact::AWAY:
		case Contact::AWAY_FROM_MOBILE:
			status = "AWAY";
			break;
		case Contact::NOT_AVAILABLE:
		case Contact::NOT_AVAILABLE_FROM_MOBILE:
			status = "NA";
			break;
		case Contact::SKYPE_ME:
		case Contact::SKYPE_ME_FROM_MOBILE:
			status = "SKYPEME";
			break;
		case Contact::DO_NOT_DISTURB:
		case Contact::DO_NOT_DISTURB_FROM_MOBILE:
			status = "DND";
			break;
		case Contact::INVISIBLE:
			status = "INVISIBLE";
			break;
		case Contact::OFFLINE:
		case Contact::OFFLINE_BUT_VM_ABLE:
		case Contact::OFFLINE_BUT_CF_ABLE:
		//case Contact::PENDINGAUTH:
		//case Contact::BLOCKED:
		default:
			status = "OFFLINE";
			break;
	}
	
	return status;
}

typedef struct {
	gpointer me;
	int prop;
	SEString value;
} SKOnChangeStructHelper;

gboolean
MyAccountOnChangeTimeout(gpointer data)
{
	SKOnChangeStructHelper *helper = (SKOnChangeStructHelper *) data;
	MyAccount *account = (MyAccount *) helper->me;
	gint prop = (int)helper->prop;
	account->OnChangeThreadSafe(prop, helper->value);
	g_free(helper);
	return FALSE;
}

void MyAccount::OnChange(int prop)
{
	SKOnChangeStructHelper *helper = g_new0(SKOnChangeStructHelper, 1);
	helper->me = (gpointer) this;
	helper->prop = prop;
	helper->value = this->GetProp(prop);

	SEStringList DebugStrings = this->getPropDebug(prop, helper->value);
	purple_debug_info("skypekit", "Account %s %s\n", (const char *)DebugStrings[1], (const char *)DebugStrings[2]);

	purple_timeout_add(1, MyAccountOnChangeTimeout, helper);
	return;
}
void MyAccount::OnChangeThreadSafe(int prop, const SEString& value)
{
	if (prop == Account::P_STATUS)
	{
		Account::STATUS status = (Account::STATUS) value.toUInt();
		size_t progress;
		const gchar *status_text;
		
		switch(status)
		{
			case Account::LOGGED_OUT:
			case Account::LOGGED_OUT_AND_PWD_SAVED:
			case Account::LOGGING_OUT:
				status_text = "Logged out";
				progress = 0;
				break;
			case Account::CONNECTING_TO_P2P:
				status_text = "Connecting to P2P network";
				progress = 1;
				break;
			case Account::CONNECTING_TO_SERVER:
				status_text = "Connecting to login server";
				progress = 2;
				break;
			case Account::LOGGING_IN:
				status_text = "Waiting for response from server";
				progress = 3;
				break;
			case Account::INITIALIZING:
				status_text = "Response OK, initialising structures";
				progress = 4;
				break;
			case Account::LOGGED_IN:
				status_text = "Logged in";
				progress = 5;
				break;
		}
		
		purple_connection_update_progress(this->pc, status_text, progress, 6);
	}
	
	if ((prop == Account::P_STATUS) && (value.toUInt() == Account::LOGGED_IN))
	{
		purple_connection_set_state(this->pc, PURPLE_CONNECTED);
		purple_debug_info("skypekit", "Login complete.\n");
		
		//Download the buddy list
		
        ContactGroup::Ref SkypeBuddies;
		Skype *skype = (Skype *)this->pc->proto_data;
        skype->GetHardwiredContactGroup(ContactGroup::ALL_BUDDIES, SkypeBuddies);
		
        ContactRefs MyContactList;
        SkypeBuddies->GetContacts(MyContactList);
		
		PurpleBuddy *buddy;
        PurpleGroup *skype_group = purple_find_group("Skype");
		int ContactCount = MyContactList.size();
        for (int I = 0; I < ContactCount; I++)
        {
			SEString SkypeName;
			MyContactList[I]->GetIdentity(SkypeName);
            SEString Alias = MyContactList[I]->GetProp(Contact::P_DISPLAYNAME);
			
			buddy = purple_find_buddy(this->pa, SkypeName);
			if (buddy == NULL)
			{
				buddy = purple_buddy_new(this->pa, SkypeName, Alias);
				if (skype_group == NULL)
				{
					skype_group = purple_group_new("Skype");
					purple_blist_add_group(skype_group, NULL);
				}
				purple_blist_add_buddy(buddy, NULL, skype_group, NULL);
			}
			
			SEString Availability = MyContactList[I]->GetProp(Contact::P_AVAILABILITY);
			const char *status = skype_contact_status_to_id(Availability.toUInt());
			if (status != NULL)
				purple_prpl_got_user_status(this->pa, (const char *)SkypeName, status, NULL);
		};
	};
	
	if ((prop == Account::P_STATUS) && (value.toUInt() == Account::LOGGED_OUT))
	{
		purple_debug_info("skypekit", "Logout complete.\n");
		purple_connection_error(this->pc, _("\nSkype program closed"));
	};
};

PurpleXfer *find_skype_xfer(MyTransfer *transfer)
{
	PurpleXfer *xfer;
	PurpleXferType purpletype;
	SEIntList Keys;
	SEIntDict Values;
	GList *xfers;
	
	Keys.append(Transfer::P_TYPE);
	Keys.append(Transfer::P_STATUS);
	Keys.append(Transfer::P_FILENAME);
	Keys.append(Transfer::P_FILEPATH);
	Keys.append(Transfer::P_FILESIZE);
	Keys.append(Transfer::P_PARTNER_HANDLE);
	Values = transfer->GetProps(Keys);
	
	purpletype = (Values.find(Transfer::P_TYPE).toUInt() == Transfer::INCOMING ? PURPLE_XFER_RECEIVE : PURPLE_XFER_SEND);
	
	if (transfer->xfer != NULL)
		return transfer->xfer;
	
	gulong transfer_filesize = atoul(Values.find(Transfer::P_FILESIZE));
	
	xfers = purple_xfers_get_all();
	for(; xfers; xfers = g_list_next(xfers))
	{
		xfer = (PurpleXfer *)xfers->data;
		if (xfer->type == purpletype &&
			xfer->account == transfer->pa &&
			xfer->size == transfer_filesize &&
			(!xfer->local_filename || 
			g_str_equal(xfer->local_filename, Values.find(Transfer::P_FILEPATH))) &&
			xfer->who && g_str_equal(xfer->who, Values.find(Transfer::P_PARTNER_HANDLE)))
		{
			purple_debug_info("skypekit", "found the xfer we're looking for\n");
			//as best as we can tell, this is the xfer we're looking for
			transfer->xfer = xfer;
			xfer->data = transfer->me();
			
			return xfer;
		}
	}
	
	//Didn't find the xfer, make a new one
	xfer = purple_xfer_new(transfer->pa, purpletype, Values.find(Transfer::P_PARTNER_HANDLE));
	
	if (xfer)
	{
		if (!Values.find(Transfer::P_FILENAME).isEmpty())
			purple_xfer_set_filename(xfer, Values.find(Transfer::P_FILENAME));
		if (!Values.find(Transfer::P_FILEPATH).isEmpty())
			purple_xfer_set_local_filename(xfer, Values.find(Transfer::P_FILEPATH));
		purple_debug_info("skypekit", "filesize %lu\n", transfer_filesize);
		purple_xfer_set_size(xfer, transfer_filesize);
		
		purple_xfer_set_init_fnc(xfer, skype_xfer_init);
		purple_xfer_set_cancel_recv_fnc(xfer, skype_xfer_cancel_recv);
		purple_xfer_set_cancel_send_fnc(xfer, skype_xfer_cancel_send);
		
		xfer->data = transfer->me();
		transfer->xfer = xfer;
		
		purple_xfer_add(xfer);
		purple_debug_info("skypekit", "had to add a new xfer\n");
	}
	
	return xfer;
}

//called when the user accepts an incomming call from the ui
static gboolean 
skype_call_accept(Conversation *conversation)
{
	conversation->JoinLiveSession();
	return FALSE;
}

//called when the user ends an incomming call from the ui
static gboolean 
skype_call_end(Conversation *conversation)
{
	conversation->LeaveLiveSession();
	return FALSE;
}

static void
skype_media_state_changed(PurpleMedia *media, PurpleMediaState state, gchar *session_id, gchar *participant, MyConversation *conversation)
{
	if (state == PURPLE_MEDIA_STATE_END)
	{
		conversation->media = NULL;
	}
}

static void
skype_stream_info_changed(PurpleMedia *media, PurpleMediaInfoType type, gchar *session_id, gchar *participant, gboolean local,
		Conversation *conversation)
{
	if (type == PURPLE_MEDIA_INFO_ACCEPT) {
		skype_call_accept(conversation);
	} else if (type == PURPLE_MEDIA_INFO_HANGUP || type == PURPLE_MEDIA_INFO_REJECT) {
		skype_call_end(conversation);
	}
}

void
skype_handle_incoming_call(PurpleConnection *pc, MyConversation::Ref &conversation, const char *who, const char *display_name)
{
	PurpleAccount *account;
	account = purple_connection_get_account(pc);
	if (purple_media_manager_get())
	{
		PurpleMedia *media;
		media = purple_media_manager_create_media(purple_media_manager_get(), account, "fsrtpconference", who, FALSE);
		if (media != NULL)
		{
			purple_media_set_prpl_data(media, conversation->me());
			conversation->media = media;
			
			purple_media_add_stream(media, "skype-audio1", who, PURPLE_MEDIA_AUDIO, FALSE, "nice", 0, NULL);
			purple_media_add_stream(media, "skype-audio2", who, PURPLE_MEDIA_AUDIO, FALSE, "rawudp", 0, NULL);
			//g_signal_emit(media, purple_media_signals[STATE_CHANGED], 0, PURPLE_MEDIA_STATE_NEW, "skype-audio", who);
			//g_signal_emit_by_name(media, "state-changed", 0, PURPLE_MEDIA_STATE_NEW, "skype-audio", who);
			
			g_signal_connect_swapped(G_OBJECT(media), "accepted", G_CALLBACK(skype_call_accept), conversation->me());
			g_signal_connect(G_OBJECT(media), "state-changed", G_CALLBACK(skype_media_state_changed), conversation->me());
			g_signal_connect(G_OBJECT(media), "stream-info", G_CALLBACK(skype_stream_info_changed), conversation->me());
		} else {
			purple_debug_info("skype_media", "purple_mmcm returned NULL\n");
		}
	} else //if (!purple_media_manager_get())
	{
		gchar *temp = g_strdup_printf(_("%s (%s) is calling you."), display_name, who);
		purple_request_action(conversation->me(), _("Incoming Call"), temp, 
								_("Do you want to accept their call?"),
								0, account, who, NULL, conversation->me(), 2, 
								_("_Accept"), G_CALLBACK(skype_call_accept), 
								_("_Reject"), G_CALLBACK(skype_call_end));
		g_free(temp);
			
	}
}

void
skype_handle_call_got_ended(PurpleConnection *pc, MyConversation::Ref &conversation)
{
	if (purple_media_manager_get())
	{
		if (conversation->media)
			purple_media_manager_remove_media(purple_media_manager_get(), conversation->media);
	} else //if (!purple_media_manager_get())
	{
		purple_request_close_with_handle(conversation->me());
	}
}

gboolean
skype_initiate_media(PurpleAccount *account, const char *who, PurpleMediaSessionType type)
{
	PurpleMedia *media;
	PurpleConnection *pc = purple_account_get_connection(account);
	Skype *skype = (Skype *)pc->proto_data;
	
	//Use skype's own audio/video stuff for now
	media = purple_media_manager_create_media(purple_media_manager_get(), account, "fsrtpconference", who, TRUE);
	MyConversation::Ref conv = find_conversation(skype, who);
	
	if (media != NULL)
	{
		conv->JoinLiveSession();
		purple_media_set_prpl_data(media, conv->me());
		conv->media = media;

		g_signal_connect_swapped(G_OBJECT(media), "accepted", G_CALLBACK(skype_call_accept), conv->me());
		g_signal_connect(G_OBJECT(media), "state-changed", G_CALLBACK(skype_media_state_changed), conv->me());
		g_signal_connect(G_OBJECT(media), "stream-info", G_CALLBACK(skype_stream_info_changed), conv->me());
	} else {
		purple_debug_info("skype_media", "media is NULL\n");
	}
	return TRUE;
}

PurpleMediaCaps
skype_get_media_caps(PurpleAccount *account, const char *who)
{
	PurpleMediaCaps caps = PURPLE_MEDIA_CAPS_NONE;
	Contact::Ref contact;
	PurpleConnection *pc = purple_account_get_connection(account);
	Skype *skype = (Skype *)pc->proto_data;
	bool hasNoAudioCap, hasVideoCap;
		
	contact = find_contact(skype, who);
	contact->HasCapability(Contact::CAPABILITY_VOICE_EVER, hasNoAudioCap);
	if (!hasNoAudioCap)
		caps = PURPLE_MEDIA_CAPS_AUDIO;
	
	//TODO: Make video work
	//contact->HasCapability(Contact::CAPABILITY_VIDEO, hasVideoCap);
	//if (hasVideoCap)
	//	caps |= PURPLE_MEDIA_CAPS_AUDIO_VIDEO;

	return caps;
}

typedef struct {
	gpointer me;
	ConversationRef conversation;
	Conversation::LIST_TYPE type;
	bool added;
} SKOnConversationListChangeStructHelper;

gboolean
MySkypeOnConversationListChangeTimeout(gpointer data)
{
	SKOnConversationListChangeStructHelper *helper = (SKOnConversationListChangeStructHelper *)data;
	if (helper)
	{
		MySkype *skype = (MySkype *) helper->me;
		if (skype)
			skype->OnConversationListChangeThreadSafe(helper->conversation, helper->type, helper->added);
	}
	g_free(helper);
	return FALSE;
}

void MySkype::OnConversationListChange(
						   const ConversationRef &conversation,
						   const Conversation::LIST_TYPE &type,
						   const bool &added)
{
	SKOnConversationListChangeStructHelper *helper = g_new0(SKOnConversationListChangeStructHelper, 1);
	helper->me = (gpointer) this;
	helper->conversation = conversation;
	helper->type = type;
	helper->added = added;
	purple_timeout_add(1, MySkypeOnConversationListChangeTimeout, helper);
}

void MySkype::OnConversationListChangeThreadSafe(
						   const ConversationRef &conv,
						   const Conversation::LIST_TYPE &type,
						   const bool &added)
{
	MyConversation::Ref conversation = conv;
	if (type == Conversation::LIVE_CONVERSATIONS)
	{
		uint LiveStatus;
		SEIntList Keys;
		SEIntDict Values;
		
		Keys.append(Conversation::P_DISPLAYNAME);
		Keys.append(Conversation::P_LOCAL_LIVESTATUS);
		Values = conversation->GetProps(Keys);
		LiveStatus = Values.find(Conversation::P_LOCAL_LIVESTATUS).toUInt();
		
		if (LiveStatus == Conversation::RINGING_FOR_ME)
		{
			ParticipantRefs CallerList;
            conversation->GetParticipants(CallerList, Conversation::OTHER_CONSUMERS);
            SEString PartList = "";
            for (unsigned I = 0; I < CallerList.size(); I++)
            {
                PartList = PartList + ", " + (const char*)CallerList[I]->GetProp(Participant::P_IDENTITY);
            }
			
			skype_handle_incoming_call(this->pc, conversation, PartList, Values.find(Conversation::P_DISPLAYNAME));
		} else if (LiveStatus == Conversation::NONE)
		{
			skype_handle_call_got_ended(this->pc, conversation);
		}
	}
}

typedef struct {
	gpointer me;
	Message::Ref message;
	bool changesInboxTimestamp;
	Message::Ref supersedesHistoryMessage;
	Conversation::Ref conversation;
} SKOnMessageStructHelper;

gboolean
MySkypeOnMessageTimeout(gpointer data)
{
	SKOnMessageStructHelper *helper = (SKOnMessageStructHelper *) data;
	if (helper)
	{
		MySkype *skype = (MySkype *) helper->me;
		if (skype)
			skype->OnMessageThreadSafe(helper->message, helper->changesInboxTimestamp, helper->supersedesHistoryMessage, helper->conversation);
	}
	g_free(helper);
	return FALSE;
}
void MySkype::OnMessage(const Message::Ref& message, const bool& changesInboxTimestamp,
							 const Message::Ref& supersedesHistoryMessage,
							 const Conversation::Ref& conversation)
{
	SKOnMessageStructHelper *helper = g_new0(SKOnMessageStructHelper, 1);
	helper->me = (gpointer) this;
	helper->message = message;
	helper->changesInboxTimestamp = changesInboxTimestamp;
	helper->supersedesHistoryMessage = supersedesHistoryMessage;
	helper->conversation = conversation;
	purple_timeout_add(1, MySkypeOnMessageTimeout, helper);
	return;
}
void MySkype::OnMessageThreadSafe(const Message::Ref& message, const bool& changesInboxTimestamp,
							 const Message::Ref& supersedesHistoryMessage,
							 const Conversation::Ref& conversation)
{
	SEIntList Keys;
	SEIntDict Values;
	int MessageType = message->GetProp(Message::P_TYPE).toUInt();
	int ConvType = conversation->GetProp(Conversation::P_TYPE).toUInt();

	SEStringList DebugStrings = message->getPropDebug(Message::P_TYPE, MessageType);
	purple_debug_info("skypekit", "MessageMsg  %s %s\n", (const char *)DebugStrings[1], (const char *)DebugStrings[2]);
	DebugStrings = conversation->getPropDebug(Conversation::P_TYPE, ConvType);
	purple_debug_info("skypekit", "MessageConv %s %s\n", (const char *)DebugStrings[1], (const char *)DebugStrings[2]);

	if (MessageType == Message::POSTED_EMOTE || MessageType == Message::POSTED_TEXT)
	{
		Keys.append(Message::P_AUTHOR);
		Keys.append(Message::P_BODY_XML);
		Keys.append(Message::P_TIMESTAMP);
		Keys.append(Message::P_ORIGINALLY_MEANT_FOR);
		Values = message->GetProps(Keys);
		gchar *message_body;

		if (MessageType == Message::POSTED_EMOTE)
		{
			const gchar *tempmsg = Values.find(Message::P_BODY_XML);
			message_body = g_strconcat("/me ", tempmsg, NULL);
		} else {
			message_body = g_strdup(Values.find(Message::P_BODY_XML));
		}
		
		purple_debug_info("skypekit", "Message %s : %s : %s\n", (const char*)Values[0], (const char*)Values[1], (const char*)Values[2]);
		
		if (ConvType == Conversation::DIALOG)
		{
			if (!Values.find(Message::P_AUTHOR).equals(this->pa->username))
			{
				serv_got_im(this->pc, Values.find(Message::P_AUTHOR), message_body, 
					PURPLE_MESSAGE_RECV, Values.find(Message::P_TIMESTAMP).toUInt());
			} else {
				//Check that we haven't sent the message from here
				//serv_got_im(this->pc, Values.find(Message::P_ORIGINALLY_MEANT_FOR), message_body,
				//	PURPLE_MESSAGE_SEND, Values.find(Message::P_TIMESTAMP).toUInt());
			}
		} else if (ConvType == Conversation::CONFERENCE)
		{
			int ConvId = conversation->getOID();
			serv_got_chat_in(this->pc, ConvId, Values.find(Message::P_AUTHOR), PURPLE_MESSAGE_RECV,
				message_body, Values.find(Message::P_TIMESTAMP).toUInt());

		}
		g_free(message_body);
	} else if (MessageType == Message::POSTED_FILES) {
		// someone sending a file
		MyTransfer::Refs transfers;
		if (message->GetTransfers(transfers)) {
			for(guint i = 0; i < transfers.size(); i++) {
				MyTransfer::Ref transfer = transfers[i];
				Keys.append(Transfer::P_TYPE);
				Keys.append(Transfer::P_STATUS);
				Keys.append(Transfer::P_FILENAME);
				Keys.append(Transfer::P_FILESIZE);
				Keys.append(Transfer::P_PARTNER_HANDLE);
				Values = transfer->GetProps(Keys);
				if (Values.find(Transfer::P_STATUS).toUInt() < Transfer::TRANSFERRING)
				{
					if (Values.find(Transfer::P_TYPE).toUInt() == Transfer::INCOMING) {
						purple_debug_info("skypekit", "New incoming file\n");
						PurpleXfer *xfer = purple_xfer_new(this->pa, PURPLE_XFER_RECEIVE, Values.find(Transfer::P_PARTNER_HANDLE));
						if (xfer)
						{
							gulong filesize = atoul(Values.find(Transfer::P_FILESIZE));
							purple_xfer_set_init_fnc(xfer, skype_xfer_init);
							purple_xfer_set_request_denied_fnc(xfer, skype_xfer_request_denied);
							purple_xfer_set_filename(xfer, Values.find(Transfer::P_FILENAME));
							purple_debug_info("skypekit", "filesize %lu\n", filesize);
							purple_xfer_set_size(xfer, filesize);
							
							xfer->data = transfer->me();
							transfer->xfer = xfer;
							
							purple_xfer_request(xfer);
						}
					} else if (Values.find(Transfer::P_TYPE).toUInt() == Transfer::OUTGOING) {
						if (!transfer->xfer)
						{
							purple_debug_info("skypekit", "no PurpleXfer for this transfer\n");
							//find the current transfer, or create one if we cant find it
							transfer->xfer = find_skype_xfer(transfer->me());
						}
					}
				}
			}
		}
	} else if (MessageType == Message::SPAWNED_CONFERENCE) {
		//Turned this im into a chat
		// see MyConversation::OnSpawnConference()
	} else if (MessageType == Message::ADDED_CONSUMERS) {
		//People added to this chat
		int ConvId = conversation->getOID();
		PurpleConversation *conv = purple_find_chat(this->pc, ConvId);
		Keys.append(Message::P_IDENTITIES);
		Values = message->GetProps(Keys);
		purple_debug_info("skypekit", "added users %s to chat %d\n", (const char *)Values.find(Message::P_IDENTITIES), ConvId);
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), Values.find(Message::P_IDENTITIES), NULL, PURPLE_CBFLAGS_NONE, TRUE);
	} else if (MessageType == Message::RETIRED_OTHERS) {
		// Other user was kicked
		int ConvId = conversation->getOID();
		PurpleConversation *conv = purple_find_chat(this->pc, ConvId);
		Keys.append(Message::P_IDENTITIES);
		Keys.append(Message::P_LEAVEREASON);
		Keys.append(Message::P_REASON);
		Values = message->GetProps(Keys);
		purple_debug_info("skypekit", "removed users %s from chat %d\n", (const char *)Values.find(Message::P_IDENTITIES), ConvId);
		purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv), Values.find(Message::P_IDENTITIES), Values.find(Message::P_REASON));
	} else if (MessageType == Message::RETIRED) {
		// User left conversation
		int ConvId = conversation->getOID();
		serv_got_chat_left(this->pc, ConvId);
	}
	
	//Mark all messages as read/consumed
	conversation->SetConsumedHorizon(time(NULL));
}

gboolean
MyConversationOnSpawnConferenceTimeout(gpointer data)
{
	MyConversation *conv = (MyConversation *) data;
	conv->OnSpawnConferenceThreadSafe(conv->ref());
	return FALSE;
}
void MyConversation::OnSpawnConference(const ConversationRef& spawned)
{
	MyConversation::Ref conv = spawned;
	purple_timeout_add(1, MyConversationOnSpawnConferenceTimeout, conv->me());
}
//ignore the 'this' keyword here, TODO fix the timeout call
void MyConversation::OnSpawnConferenceThreadSafe(const ConversationRef& spawned)
{
	SEIntList Keys;
	SEIntDict Values;
	MyConversation::Ref conversation = spawned;
	int ConvId = conversation->getOID();
	
	Keys.append(Conversation::P_IDENTITY);
	Keys.append(Conversation::P_DISPLAYNAME);
	Values = conversation->GetProps(Keys);
	
	purple_debug_info("skypekit", "joined chat %s %d\n", (const char *)Values.find(Conversation::P_IDENTITY), ConvId);
	PurpleConversation *conv = serv_got_joined_chat(this->pc, ConvId, Values.find(Conversation::P_IDENTITY));
	purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), NULL, Values.find(Conversation::P_DISPLAYNAME));
	
	ParticipantRefs participants;
	conversation->GetParticipants(participants);
	ParticipantRef participant;
	while((participant = participants.peek()))
	{
		SEString who;
		participant->GetPropIdentity(who);
		purple_debug_info("skypekit", "adding user %s to chat %d\n", (const char *) who, ConvId);
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), who, NULL, PURPLE_CBFLAGS_NONE, FALSE);
	}
}

gboolean
MyConversationOnChangeTimeout(gpointer data)
{
	SKOnChangeStructHelper *helper = (SKOnChangeStructHelper *) data;
	MyConversation *conversation = (MyConversation *) helper->me;
	int prop = (int) helper->prop;
	conversation->OnChangeThreadSafe(prop, helper->value);
	g_free(helper);
	return FALSE;
}
void MyConversation::OnChange(int prop)
{
	SKOnChangeStructHelper *helper = g_new0(SKOnChangeStructHelper, 1);
	helper->me = (gpointer) this;
	helper->prop = prop;
	helper->value = this->GetProp(prop);

	SEStringList DebugStrings = this->getPropDebug(prop, helper->value);
	purple_debug_info("skypekit", "Conversation %s %s\n", (const char *)DebugStrings[1], (const char *)DebugStrings[2]);

	purple_timeout_add(1, MyConversationOnChangeTimeout, helper);
	return;
}
void MyConversation::OnChangeThreadSafe(int prop, const SEString &value)
{
}

gboolean
MyMessageOnChangeTimeout(gpointer data)
{
	SKOnChangeStructHelper *helper = (SKOnChangeStructHelper *) data;
	MyMessage *message = (MyMessage *) helper->me;
	int prop = (int) helper->prop;
	message->OnChangeThreadSafe(prop, helper->value);
	g_free(helper);
	return FALSE;
}
void MyMessage::OnChange(int prop)
{
	SKOnChangeStructHelper *helper = g_new0(SKOnChangeStructHelper, 1);
	helper->me = (gpointer) this;
	helper->prop = prop;
	helper->value = this->GetProp(prop);

	SEStringList DebugStrings = this->getPropDebug(prop, helper->value);
	purple_debug_info("skypekit", "Message %s %s\n", (const char *)DebugStrings[1], (const char *)DebugStrings[2]);

	purple_timeout_add(1, MyMessageOnChangeTimeout, helper);
	return;
}
void MyMessage::OnChangeThreadSafe(int prop, const SEString &value)
{
	SEIntList Keys;
	SEIntDict Values;
	Keys.append(Message::P_ORIGINALLY_MEANT_FOR);
	Keys.append(Message::P_BODY_XML);
	Keys.append(Message::P_CONVO_ID);
	int MsgId = this->getOID();

	if (prop == Message::P_SENDING_STATUS)
	{
		Values = this->GetProps(Keys);

		const gchar *tempmsg = Values.find(Message::P_BODY_XML);
		const gchar *otherperson = Values.find(Message::P_ORIGINALLY_MEANT_FOR);
		gchar *message_number = g_strdup_printf("%du", MsgId);
		
		if (value.toUInt() == Message::SENT)
		{
			purple_signal_emit(purple_conversations_get_handle(), "received-im-ack",
				this->pa, otherperson, message_number);
		} else if (value.toUInt() == Message::SENDING)
		{
			purple_signal_emit(purple_conversations_get_handle(), "waiting-im-ack",
				this->pa, otherperson, tempmsg, message_number);
		}
		
		g_free(message_number);
	}
}

gboolean
MyParticipantOnChangeTimeout(gpointer data)
{
	SKOnChangeStructHelper *helper = (SKOnChangeStructHelper *) data;
	MyParticipant *participant = (MyParticipant *) helper->me;
	int prop = (int) helper->prop;
	participant->OnChangeThreadSafe(prop, helper->value);
	g_free(helper);
	return FALSE;
}
void MyParticipant::OnChange(int prop)
{
	SKOnChangeStructHelper *helper = g_new0(SKOnChangeStructHelper, 1);
	helper->me = (gpointer) this;
	helper->prop = prop;
	helper->value = this->GetProp(prop);

	SEStringList DebugStrings = this->getPropDebug(prop, helper->value);
	purple_debug_info("skypekit", "Participant %s %s\n", (const char *)DebugStrings[1], (const char *)DebugStrings[2]);

	purple_timeout_add(1, MyParticipantOnChangeTimeout, helper);
	return;
}
void MyParticipant::OnChangeThreadSafe(int prop, const SEString &value)
{
	if (prop == Participant::P_TEXT_STATUS)
	{
		// typing notifications
		int TextStatus = value.toUInt();
		PurpleTypingState typingState;
		switch(TextStatus)
		{
			default:
			case Participant::TEXT_NA:
				typingState = PURPLE_NOT_TYPING;
				break;
			case Participant::READING:
				typingState = PURPLE_TYPED;
				break;
			case Participant::WRITING:
			case Participant::WRITING_AS_ANGRY:
			case Participant::WRITING_AS_CAT:
				typingState = PURPLE_TYPING;
				break;
		}
	    
		SEString Name = this->GetProp(Participant::P_IDENTITY);
		purple_debug_info("skypekit", "Got typing notification from %s\n", (const char *) Name);
		serv_got_typing(this->pc, (const char *)Name, 10, typingState);
	}
}

gboolean
MyTransferOnChangeTimeout(gpointer data)
{
	SKOnChangeStructHelper *helper = (SKOnChangeStructHelper *) data;
	MyTransfer *transfer = (MyTransfer *) helper->me;
	int prop = (int) helper->prop;
	transfer->OnChangeThreadSafe(prop, helper->value);
	g_free(helper);
	return FALSE;
}
void MyTransfer::OnChange(int prop)
{
	SKOnChangeStructHelper *helper = g_new0(SKOnChangeStructHelper, 1);
	helper->me = (gpointer) this;
	helper->prop = prop;
	helper->value = this->GetProp(prop);

	SEStringList DebugStrings = this->getPropDebug(prop, helper->value);
	purple_debug_info("skypekit", "Transfer %s %s\n", (const char *)DebugStrings[1], (const char *)DebugStrings[2]);
	
	purple_timeout_add(1, MyTransferOnChangeTimeout, helper);
	return;
}
void MyTransfer::OnChangeThreadSafe(int prop, const SEString &value)
{
	if (this->xfer == NULL)
	{
		this->xfer = find_skype_xfer(this);
	}
	
	if (this->xfer != NULL)
	{
		if(prop == Transfer::P_STATUS)
		{
			switch(value.toUInt())
			{
				case Transfer::NEW:
				case Transfer::WAITING_FOR_ACCEPT:
					purple_xfer_set_status(this->xfer, PURPLE_XFER_STATUS_NOT_STARTED);
					break;
				case Transfer::CONNECTING:
					purple_xfer_set_status(this->xfer, PURPLE_XFER_STATUS_ACCEPTED);
					break;
				case Transfer::TRANSFERRING:
				case Transfer::TRANSFERRING_OVER_RELAY:
					purple_xfer_set_status(this->xfer, PURPLE_XFER_STATUS_STARTED);
					break;
				case Transfer::COMPLETED:
					purple_xfer_set_completed(this->xfer, TRUE);
					break;
				case Transfer::CANCELLED_BY_REMOTE:
					purple_xfer_cancel_remote(this->xfer);
					break;
				case Transfer::CANCELLED:
					purple_xfer_cancel_local(this->xfer);
					break;
				case Transfer::FAILED:
					purple_xfer_error(this->xfer->type, this->pa, this->xfer->who, "Transfer failed");
					break;
				case Transfer::PAUSED:
				case Transfer::REMOTELY_PAUSED:
				case Transfer::PLACEHOLDER:
				case Transfer::OFFER_FROM_OTHER_INSTANCE:
				default:
					break;
			}
			purple_xfer_update_progress(this->xfer);
		} else if (prop == Transfer::P_BYTESTRANSFERRED)
		{
			if (purple_xfer_get_status(this->xfer) < PURPLE_XFER_STATUS_STARTED)
				purple_xfer_set_status(this->xfer, PURPLE_XFER_STATUS_STARTED);
			purple_xfer_set_bytes_sent(this->xfer, atol(value));
			purple_xfer_update_progress(this->xfer);
		}
	}
}

void
skypekit_auth_allow(gpointer userdata)
{
	ContactRef contact = * (ContactRef *)userdata;
	contact->SetBuddyStatus(true);
}
void
skypekit_auth_deny(gpointer userdata)
{
	ContactRef contact = * (ContactRef *)userdata;
	contact->SetBlocked(true);
}

static void
skype_call_user_from_blist(PurpleBlistNode *node, Conversation *conv)
{
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		conv->JoinLiveSession();
	}
}
static void
skype_hangup_from_blist(PurpleBlistNode *node, Conversation *conv)
{
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		conv->LeaveLiveSession();
	}
}

static void
skype_request_auth_from_blist(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;

	if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
		return;
	
	buddy = (PurpleBuddy *)node;

	PurpleConnection *pc = purple_account_get_connection(buddy->account);
	Skype *skype = (Skype *)pc->proto_data;
	Contact::Ref contact = find_contact(skype, buddy->name);

	contact->SendAuthRequest("");
}

static GList *
skype_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		PurpleBuddy *buddy = (PurpleBuddy *)node;
		PurpleConnection *pc = purple_account_get_connection(buddy->account);
		Skype *skype = (Skype *)pc->proto_data;
		
		if (!purple_media_manager_get())
		{
			MyConversation::Ref conv = find_conversation(skype, buddy->name);
			Conversation::LOCAL_LIVESTATUS liveStatus;
			conv->GetPropLocalLivestatus(liveStatus);
			
			if (liveStatus == Conversation::NONE || 
				liveStatus == Conversation::OTHERS_ARE_LIVE || 
				liveStatus == Conversation::RECENTLY_LIVE)
			{
				act = purple_menu_action_new(_("Call..."),
												PURPLE_CALLBACK(skype_call_user_from_blist),
												conv->me(), NULL);
				m = g_list_append(m, act);
			} else if (liveStatus != Conversation::OTHERS_ARE_LIVE_FULL)
			{
				act = purple_menu_action_new(_("End Call..."),
												PURPLE_CALLBACK(skype_hangup_from_blist),
												conv->me(), NULL);
				m = g_list_append(m, act);
			
			}
		}
		
		if (!PURPLE_BUDDY_IS_ONLINE(buddy))
		{
			Contact::Ref contact = find_contact(skype, buddy->name);
			Contact::AVAILABILITY avail;
			contact->GetPropAvailability(avail);
			
			if (avail == Contact::PENDINGAUTH)
			{
				act = purple_menu_action_new(_("Re-request authorization"),
								PURPLE_CALLBACK(skype_request_auth_from_blist),
								NULL, NULL);
				m = g_list_append(m, act);
			}
		}
	}
	
	return m;
}


typedef struct {
	gpointer me;
	ContactRef contact;
} SKOnContactGroupChangeStructHelper;
gboolean
MyContactGroupOnChangeTimeout(gpointer data)
{
	SKOnContactGroupChangeStructHelper *helper = (SKOnContactGroupChangeStructHelper *) data;
	MyContactGroup *contactgroup = (MyContactGroup *) helper->me;
	contactgroup->OnChangeThreadSafe(helper->contact);
	g_free(helper);
	return FALSE;
}
void MyContactGroup::OnChange(const ContactRef& contact)
{
	SKOnContactGroupChangeStructHelper *helper = g_new0(SKOnContactGroupChangeStructHelper, 1);
	helper->me = (gpointer) this;
	helper->contact = contact;
	purple_timeout_add(1, MyContactGroupOnChangeTimeout, helper);
	return;
}
void MyContactGroup::OnChangeThreadSafe(const ContactRef& contact)
{
	purple_debug_info("skypekit", "ContactGroup::OnChange\n");
	
	TYPE type;
	this->GetPropType(type);
	if (type == ContactGroup::CONTACTS_WAITING_MY_AUTHORIZATION)
	{
		SEString ContactSkypeName;
		SEString ContactDisplayName;
		SEString AuthRequestText;
		contact->GetPropSkypename(ContactSkypeName);
		contact->GetPropReceivedAuthrequest(AuthRequestText);
		contact->GetPropDisplayname(ContactDisplayName);

		purple_account_request_authorization(this->pa, ContactSkypeName, NULL, ContactDisplayName, 
					AuthRequestText, FALSE, skypekit_auth_allow, skypekit_auth_deny,
					&(contact->ref()));

		//contact->IgnoreAuthRequest();
	}
}

gboolean
MyContactOnChangeTimeout(gpointer data)
{
	SKOnChangeStructHelper *helper = (SKOnChangeStructHelper *) data;
	MyContact *contact = (MyContact *) helper->me;
	int prop = (int) helper->prop;
	contact->OnChangeThreadSafe(prop, helper->value);
	g_free(helper);
	return FALSE;
}
void MyContact::OnChange(int prop)
{
	SKOnChangeStructHelper *helper = g_new0(SKOnChangeStructHelper, 1);
	helper->me = (gpointer) this;
	helper->prop = prop;
	helper->value = this->GetProp(prop);

	SEStringList DebugStrings = this->getPropDebug(prop, helper->value);
	purple_debug_info("skypekit", "Contact %s %s\n", (const char *)DebugStrings[1], (const char *)DebugStrings[2]);
	
	purple_timeout_add(1, MyContactOnChangeTimeout, helper);
	return;
}

void
skype_auth_allow(gpointer pointer)
{
	Contact *contact = (Contact *)pointer;
	//contact->GiveAuthlevel(Contact::AUTHORIZED_BY_ME);
	contact->SetBuddyStatus(true);
}

void
skype_auth_deny(gpointer pointer)
{
	Contact *contact = (Contact *)pointer;
	//contact->GiveAuthlevel(Contact::BLOCKED_BY_ME);
	contact->SetBlocked(true);
}

void MyContact::OnChangeThreadSafe(int prop, const SEString &value)
{
	SEString Name;
	if (prop == Contact::P_AVAILABILITY)
	{
	    this->GetIdentity(Name);
//        SEStringList DebugStrings = this->getPropDebug(prop, value);
//		purple_debug_info("skypekit", "User %s is %s\n", (const char *)Name, (const char *)DebugStrings[2]);
		const char *status;
		
		status = skype_contact_status_to_id(value.toUInt());
		if (status != NULL)
			purple_prpl_got_user_status(this->pa, (const char *)Name, status, NULL);
	} else if (prop == Contact::P_AVATAR_IMAGE)
	{
		bool hasAvatar = false;
		Sid::Binary avatarData;
	    this->GetIdentity(Name);
		this->GetAvatar(hasAvatar, avatarData);
		if (hasAvatar)
		{
			purple_buddy_icons_set_for_user(this->pa, (const char *)Name, (char *)avatarData, avatarData.size(), NULL);
		} else {
			purple_buddy_icons_set_for_user(this->pa, (const char *)Name, NULL, 0, NULL);
		}
	} else if (prop == Contact::P_MOOD_TEXT)
	{
		//moodString = new SEString(value);
	} else if (prop == Contact::P_RECEIVED_AUTHREQUEST)
	{
		SEString DisplayName;
		this->GetPropDisplayname(DisplayName);
	    this->GetIdentity(Name);
		
		if (purple_account_get_bool(this->pa, "reject_all_auths", FALSE))
		{
			skype_auth_deny(this->me());
		} else {
			purple_account_request_authorization(this->pa, (const char *)Name, NULL, (const char *)DisplayName,
													value, (purple_find_buddy(this->pa, (const char *)Name) != NULL),
													skype_auth_allow, skype_auth_deny, this->me());
		}
	}
};

