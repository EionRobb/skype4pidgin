
#include "prpl.h"
#include "blist.h"
#include "account.h"
#include "accountopt.h"
#include "connection.h"
#include "server.h"

#include "mediamanager.h"
#include "request.h"

#include <glib.h>

#define SKYPE_APP_TOKEN "AAAgBobH4q2OPAaCX6vWseC82MmHZ1Cpayj61rbYlh0uenHxFByJ/lLu9HSN5nT3TjS91/2RQMASlCmZUCM5zINkR3nQ1240JpB0yNfYfzxXm8EyE9p9gWAGU7spUMvuROxoQR0042VUR4dCRW/kYr3yeYiYOXW0poxxwg+esEbX8W1tqing25kfjUVsij6+T+dxtV8t/B1yGpTiT1okj9FoBvZgnwDoEGEywG5xeJTGLuFtHGALqa7gwvj9rulf7TuM1Q=="

#define GETTEXT_PACKAGE "skype4pidgin"
#ifdef ENABLE_NLS
#	ifdef _WIN32
#		include <win32dep.h>
#	endif
#	include <glib/gi18n-lib.h>
#else
#	define _(a) a
#endif

#include "debug.h"

#include "skype-embedded_2.h"
#include "skype-tcp-transport.h"


#define atoul(x) strtoul(x, (char **)NULL, 10)


class MyAccount : public Account
{
public:
	typedef DRef<MyAccount, Account> Ref;
	MyAccount(unsigned int oid, SERootObject* root);
	~MyAccount() {};
	PurpleAccount *pa;
	PurpleConnection *pc;

	void OnChange(int prop);
	void OnChangeThreadSafe(int prop, const SEString& value);
};

MyAccount::MyAccount(unsigned int oid, SERootObject* root) : Account(oid, root) {};


class MyContact : public Contact
{
public:
    typedef DRef<MyContact, Contact> Ref;
    MyContact(unsigned int oid, SERootObject* root);
    ~MyContact() {};
	PurpleAccount *pa;
	PurpleConnection *pc;
	MyContact *me() {return this;}
	SEString moodString;
	
    void OnChange(int prop);
    void OnChangeThreadSafe(int prop, const SEString& value);
};

MyContact::MyContact(unsigned int oid, SERootObject* root) : Contact(oid, root) {};

class MyContactGroup : public ContactGroup
{
public:
    typedef DRef<MyContactGroup, ContactGroup> Ref;
    MyContactGroup(unsigned int oid, SERootObject* root);
    ~MyContactGroup() {};
	PurpleAccount *pa;
	PurpleConnection *pc;
	
    void OnChange(const ContactRef& contact);
    void OnChangeThreadSafe(const ContactRef& contact);
};

MyContactGroup::MyContactGroup(unsigned int oid, SERootObject* root) : ContactGroup(oid, root) {};

class MyParticipant : public Participant
{
public:
    typedef DRef<MyParticipant, Participant> Ref;
    MyParticipant(unsigned int oid, SERootObject* root);
    ~MyParticipant() {};
	PurpleAccount *pa;
	PurpleConnection *pc;
	
    void OnChange(int prop);
    void OnChangeThreadSafe(int prop, const SEString& value);
};

MyParticipant::MyParticipant(unsigned int oid, SERootObject* root) : Participant(oid, root) {};

class MyTransfer : public Transfer
{
public:
    typedef DRef<MyTransfer, Transfer> Ref;
    MyTransfer(unsigned int oid, SERootObject* root);
    ~MyTransfer() {};
	PurpleAccount *pa;
	PurpleConnection *pc;
	PurpleXfer *xfer;
	
	MyTransfer *me() {return this;}
	
    void OnChange(int prop);
    void OnChangeThreadSafe(int prop, const SEString& value);
};

MyTransfer::MyTransfer(unsigned int oid, SERootObject* root) : Transfer(oid, root) {};

class MyConversation : public Conversation
{
public:
    typedef DRef<MyConversation, Conversation> Ref;
    MyConversation(unsigned int oid, SERootObject* root);
    ~MyConversation() {};
	PurpleAccount *pa;
	PurpleConnection *pc;
	PurpleMedia *media;
	
	MyConversation *me() {return this;}
	
    void OnChange(int prop);
    void OnChangeThreadSafe(int prop, const SEString& value);
	void OnSpawnConference(const ConversationRef& spawned);
	void OnSpawnConferenceThreadSafe(const ConversationRef& spawned);
};

MyConversation::MyConversation(unsigned int oid, SERootObject* root) : Conversation(oid, root) {};

class MyMessage : public Message
{
public:
    typedef DRef<MyMessage, Message> Ref;
    MyMessage(unsigned int oid, SERootObject* root);
    ~MyMessage() {};
	PurpleAccount *pa;
	PurpleConnection *pc;
	
    void OnChange(int prop);
    void OnChangeThreadSafe(int prop, const SEString& value);
};

MyMessage::MyMessage(unsigned int oid, SERootObject* root) : Message(oid, root) {};

class MySkype : public Skype
{
public:
    MySkype(SETransport* transport) : Skype(transport) {}
    ~MySkype() {}
    PurpleConnection *pc;
    PurpleAccount *pa;
	
    // Every time an account object is created, we will return instance of MyAccount
    Account* newAccount(int oid) {MyAccount *acc = new MyAccount(oid, this); acc->pc=this->pc; acc->pa=this->pa; return acc;}
    Contact* newContact(int oid) {MyContact *con = new MyContact(oid, this); con->pc=this->pc; con->pa=this->pa; return con;}
    ContactGroup* newContactGroup(int oid) {MyContactGroup *grp = new MyContactGroup(oid, this); grp->pc=this->pc; grp->pa=this->pa; return grp;}
    Participant* newParticipant(int oid) {MyParticipant *par = new MyParticipant(oid, this); par->pc=this->pc; par->pa=this->pa; return par;}
    Transfer* newTransfer(int oid) {MyTransfer *tra = new MyTransfer(oid, this); tra->xfer=NULL; tra->pc=this->pc; tra->pa=this->pa; return tra;}
    Conversation* newConversation(int oid) {MyConversation *con = new MyConversation(oid, this); con->pc=this->pc; con->pa=this->pa; return con;}
    Message* newMessage(int oid) {MyMessage *msg = new MyMessage(oid, this); msg->pc=this->pc; msg->pa=this->pa; return msg;}

    virtual void OnMessage(
						   const Message::Ref& message,
						   const bool& changesInboxTimestamp,
						   const Message::Ref& supersedesHistoryMessage,
						   const Conversation::Ref& conversation);
    virtual void OnMessageThreadSafe(
						   const Message::Ref& message,
						   const bool& changesInboxTimestamp,
						   const Message::Ref& supersedesHistoryMessage,
						   const Conversation::Ref& conversation);
	
	virtual void OnConversationListChange(
						   const ConversationRef &conversation,
						   const Conversation::LIST_TYPE &type,
						   const bool &added);
	virtual void OnConversationListChangeThreadSafe(
						   const ConversationRef &conversation,
						   const Conversation::LIST_TYPE &type,
						   const bool &added);
};

extern "C" {
	
	const char *skype_list_icon(PurpleAccount *account, PurpleBuddy *buddy);
	GList *skype_status_types(PurpleAccount *acct);
	void skype_login(PurpleAccount *acct);
	void skype_close(PurpleConnection *gc);
	int skype_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags);
	void skype_set_status(PurpleAccount *account, PurpleStatus *status);
	unsigned int skype_send_typing(PurpleConnection *gc, const char *name, PurpleTypingState state);
	void skype_add_deny(PurpleConnection *gc, const char *who);
	void skype_rem_deny(PurpleConnection *gc, const char *who);
	char *skype_status_text(PurpleBuddy *buddy);
	void skype_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
	void skype_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
	void skype_add_permit(PurpleConnection *gc, const char *who);
	void skype_rem_permit(PurpleConnection *gc, const char *who);
	const char *skype_list_emblem(PurpleBuddy *buddy);
	void skype_alias_buddy(PurpleConnection *gc, const char *who, const char *alias);
	void skype_send_file(PurpleConnection *, const char *who, const char *filename);
	gboolean skype_offline_msg(const PurpleBuddy *buddy);
	gboolean skype_can_receive_file(PurpleConnection *pc, const char *who);
	PurpleXfer *skype_xfer_new(PurpleConnection *pc, const char *who);
	void skype_alias_buddy(PurpleConnection *pc, const char *who, const char *alias);
	void skype_set_buddy_icon(PurpleConnection *pc, PurpleStoredImage *img);
	gboolean skype_initiate_media(PurpleAccount *account, const char *who, PurpleMediaSessionType type);
	PurpleMediaCaps skype_get_media_caps(PurpleAccount *account, const char *who);
	static GList *skype_node_menu(PurpleBlistNode *node);
	void skype_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *userinfo, gboolean full);
	int skype_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags);
	const char *skype_normalize(const PurpleAccount *acct, const char *who);
	void skype_set_chat_topic(PurpleConnection *gc, int id, const char *topic);
	void skype_get_info(PurpleConnection *gc, const gchar *username);
	
	PurplePluginProtocolInfo prpl_info = {
		/* options */
		(PurpleProtocolOptions) (OPT_PROTO_CHAT_TOPIC | OPT_PROTO_USE_POINTSIZE),
		NULL,                /* user_splits */
		NULL,                /* protocol_options */
		{"jpeg", 0, 0, 256, 256, 0, PURPLE_ICON_SCALE_SEND}, /* icon_spec */
		skype_list_icon,     /* list_icon */
		skype_list_emblem,   /* list_emblem */
		skype_status_text,   /* status_text */
		skype_tooltip_text,  /* tooltip_text */
		skype_status_types,  /* status_types */
		skype_node_menu,     /* blist_node_menu */
		NULL,/* chat_info */
		NULL,/* chat_info_defaults */
		skype_login,         /* login */
		skype_close,         /* close */
		skype_send_im,       /* send_im */
		NULL,                /* set_info */
		skype_send_typing,   /* send_typing */
		skype_get_info,      /* get_info */
		skype_set_status,    /* set_status */
		NULL,      /* set_idle */
		NULL,                /* change_passwd */
		skype_add_buddy,     /* add_buddy */
		NULL,                /* add_buddies */
		skype_remove_buddy,  /* remove_buddy */
		NULL,                /* remove_buddies */
		skype_add_permit,    /* add_permit */
		skype_add_deny,      /* add_deny */
		skype_rem_permit,    /* rem_permit */
		skype_rem_deny,      /* rem_deny */
		NULL,                /* set_permit_deny */
		NULL,     /* join_chat */
		NULL,                /* reject chat invite */
		NULL, /* get_chat_name */
		NULL,   /* chat_invite */
		NULL,    /* chat_leave */
		NULL,                /* chat_whisper */
		skype_chat_send,     /* chat_send */
		NULL,     /* keepalive */
		NULL,                /* register_user */
		NULL,                /* get_cb_info */
		NULL,                /* get_cb_away */
		skype_alias_buddy,   /* alias_buddy */
		NULL,   /* group_buddy */
		NULL,  /* rename_group */
		NULL,    /* buddy_free */
		NULL,                /* convo_closed */
		skype_normalize,     /* normalize */
		skype_set_buddy_icon,/* set_buddy_icon */
		NULL,  /* remove_group */
		NULL,  /* get_cb_real_name */
		skype_set_chat_topic,/* set_chat_topic */
		NULL,				 /* find_blist_chat */
		NULL,                /* roomlist_get_list */
		NULL,                /* roomlist_cancel */
		NULL,                /* roomlist_expand_category */
		skype_can_receive_file, /* can_receive_file */
		skype_send_file,     /* send_file */
		skype_xfer_new,      /* new_xfer */
		skype_offline_msg,   /* offline_message */
		NULL,                /* whiteboard_prpl_ops */
		NULL,      /* send_raw */
		NULL,                /* roomlist_room_serialize */
		NULL,                /* unregister_user */
		NULL,                /* send_attention */
		NULL,                /* attention_types */
#if PURPLE_MAJOR_VERSION == 2 && PURPLE_MINOR_VERSION == 1
		(gpointer)
#endif
		sizeof(PurplePluginProtocolInfo), /* struct_size */
		NULL,                /* get_account_text_table */
		skype_initiate_media,/* initiate_media */
		skype_get_media_caps /* can_do_media */
	};
	
	gboolean
	plugin_load(PurplePlugin *plugin)
	{
		return TRUE;
	}
	
	gboolean
	plugin_unload(PurplePlugin *plugin)
	{
		return TRUE;
	}

	static PurplePluginInfo info = {
		PURPLE_PLUGIN_MAGIC,
		/*	PURPLE_MAJOR_VERSION,
		 PURPLE_MINOR_VERSION,
		 */
		2, 1,
		PURPLE_PLUGIN_PROTOCOL, /* type */
		NULL, /* ui_requirement */
		0, /* flags */
		NULL, /* dependencies */
		PURPLE_PRIORITY_DEFAULT, /* priority */
		"prpl-bigbrownchunx-skypekit", /* id */
		"SkypeKit", /* name */
		"0.1", /* version */
		"Allows using Skype IM functions from within Pidgin", /* summary */
		"Allows using Skype IM functions from within Pidgin", /* description */
		"Eion Robb <eion@robbmob.com>", /* author */
		"http://eion.robbmob.com/", /* homepage */
		plugin_load, /* load */
		plugin_unload, /* unload */
		NULL, /* destroy */
		NULL, /* ui_info */
		&prpl_info, /* extra_info */
		NULL, /* prefs_info */
		NULL, /* actions */
		NULL, /* padding */
		NULL,
		NULL,
		NULL
	};
	
	static void
	plugin_init(PurplePlugin *plugin)
	{
		PurpleAccountOption *option;
		
		#if _WIN32 && ENABLE_NLS
			bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
			bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
		#endif
				
		//if (!g_thread_supported ())
			//g_thread_init (NULL);

		option = purple_account_option_string_new(_("Server"), "host", "127.0.0.1");
		prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
		option = purple_account_option_int_new(_("Port"), "port", 8963);
		prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	}															

	PURPLE_INIT_PLUGIN(skypekit, plugin_init, info);
};
