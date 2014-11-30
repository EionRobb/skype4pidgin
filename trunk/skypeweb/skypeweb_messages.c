#include "skypeweb_messages.h"
#include "skypeweb_util.h"
#include "skypeweb_connection.h"

static void
process_userpresence_resource(SkypeWebAccount *sa, JsonObject *resource)
{
	const gchar *selfLink = json_object_get_string_member(resource, "selfLink");
	const gchar *status = json_object_get_string_member(resource, "status");
	const gchar *from;
	
	from = skypeweb_contact_url_to_name(selfLink);
	
	//TODO not need me
	if (!purple_find_buddy(sa->account, from))
	{
		PurpleGroup *group = purple_find_group("Skype");
		if (!group)
		{
			group = purple_group_new("Skype");
			purple_blist_add_group(group, NULL);
		}
		purple_blist_add_buddy(purple_buddy_new(sa->account, from, NULL), NULL, group, NULL);
	}
	
	purple_prpl_got_user_status(sa->account, from, status, NULL);
}

static void
process_message_resource(SkypeWebAccount *sa, JsonObject *resource)
{
	const gchar *messagetype = json_object_get_string_member(resource, "messagetype");
	const gchar *from = json_object_get_string_member(resource, "from");
	const gchar *content = json_object_get_string_member(resource, "content");
	const gchar *composetime = json_object_get_string_member(resource, "composetime");
	const gchar *conversationLink = json_object_get_string_member(resource, "conversationLink");
	time_t composetimestamp = purple_str_to_time(composetime, TRUE, NULL, NULL, NULL);
	gchar **messagetype_parts;
	
	messagetype_parts = g_strsplit(messagetype, "/", -1);
	
	from = skypeweb_contact_url_to_name(from);
	
	if (json_object_has_member(resource, "threadtopic"))
		return;  //TODO multi user chat threads's
	
	if (g_str_equal(messagetype_parts[0], "Control")) {
		if (g_str_equal(messagetype_parts[1], "ClearTyping")) {
			serv_got_typing(sa->pc, from, 20, PURPLE_NOT_TYPING);
		} else if (g_str_equal(messagetype_parts[1], "Typing")) {
			serv_got_typing(sa->pc, from, 20, PURPLE_TYPING);
		}
	} else if (g_str_equal(messagetype, "RichText") || g_str_equal(messagetype, "Text")) {
		gchar *html;
		if (g_str_equal(messagetype, "Text")) {
			html = purple_markup_escape_text(content, -1);
		} else {
			html = g_strdup(content);
		}
		if (g_str_equal(sa->account->username, from)) {
			from = skypeweb_contact_url_to_name(conversationLink);
			PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, from, sa->account);
			if (conv == NULL)
			{
				conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, sa->account, from);
			}
			purple_conversation_write(conv, from, html, PURPLE_MESSAGE_SEND, composetimestamp);
		} else {
			serv_got_im(sa->pc, from, html, PURPLE_MESSAGE_RECV, composetimestamp);
		}
		g_free(html);
	} else {
		purple_debug_warning("skypeweb", "Unknown message resource messagetype '%s'\n", messagetype);
	}
	
	g_strfreev(messagetype_parts);
}

gboolean
skypeweb_timeout(gpointer userdata)
{
	SkypeWebAccount *sa = userdata;
	skypeweb_poll(sa);
	
	// If no response within 3 minutes, assume connection lost and try again
	purple_timeout_remove(sa->watchdog_timeout);
	sa->watchdog_timeout = purple_timeout_add_seconds(3 * 60, skypeweb_timeout, sa);
	
	return FALSE;
}

static void
skypeweb_poll_cb(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonArray *messages = NULL;
	guint index, length;
	JsonObject *obj;
	
	obj = json_node_get_object(node);
	
	if (json_object_has_member(obj, "eventMessages"))
		messages = json_object_get_array_member(obj, "eventMessages");
	
	if (messages != NULL) {
		length = json_array_get_length(messages);
		for(index = 0; index < length; index++)
		{
			JsonObject *message = json_array_get_object_element(messages, index);
			const gchar *resourceType = json_object_get_string_member(message, "resourceType");
			JsonObject *resource = json_object_get_object_member(message, "resource");
			
			if (g_str_equal(resourceType, "NewMessage"))
			{
				process_message_resource(sa, resource);
			} else if (g_str_equal(resourceType, "UserPresence"))
			{
				process_userpresence_resource(sa, resource);
			} else if (g_str_equal(resourceType, "EndpointPresence"))
			{
				//process_endpointpresence_resource(sa, resource);
			} else if (g_str_equal(resourceType, "ConversationUpdate"))
			{
				//process_conversation_resource(sa, resource);
			} else if (g_str_equal(resourceType, "ThreadUpdate"))
			{
				//process_thread_resource(sa, resource);
			}
		}
	}
	
	//TODO record id of highest recieved id to make sure we dont process the same id twice
	
	sa->poll_timeout = purple_timeout_add_seconds(1, skypeweb_timeout, sa);
			
}

void
skypeweb_poll(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, SKYPEWEB_MESSAGES_HOST, "/v1/users/ME/endpoints/SELF/subscriptions/0/poll", NULL, skypeweb_poll_cb, NULL, TRUE);
}


static void
skypeweb_subscribe_cb(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	//{"id":"messagingService","type":"EndpointPresenceDoc","selfLink":"uri","privateInfo":{"epname":"skype"},"publicInfo":{"capabilities":"video|audio","type":1,"skypeNameVersion":"908/1.0.20/swx-skype.com","nodeInfo":"xx","version":"908/1.0.20"}}
	// /v1/users/ME/endpoints/%7B60da4fa8-79f0-445b-846f-1f1e0116ecb5%7D/presenceDocs/messagingService
	
	skypeweb_do_all_the_things(sa);
}

static void
skypeweb_subscribe(SkypeWebAccount *sa)
{
	JsonObject *obj;
	JsonArray *interested;
	gchar *post;

	interested = json_array_new();
	json_array_add_string_element(interested, "/v1/users/ME/conversations/ALL/properties");
	json_array_add_string_element(interested, "/v1/users/ME/conversations/ALL/messages");
	json_array_add_string_element(interested, "/v1/users/ME/contacts/ALL");
	//json_array_add_string_element(interested, "/v1/users/ME/conversations/ALL/properties"); //TODO
	
	obj = json_object_new();
	json_object_set_array_member(obj, "interestedResources", interested);
	json_object_set_string_member(obj, "template", "raw");
	json_object_set_string_member(obj, "channelType", "httpLongPoll");
	
	post = skypeweb_jsonobj_to_string(obj);

	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, SKYPEWEB_MESSAGES_HOST, "/v1/users/ME/endpoints/SELF/subscriptions", post, skypeweb_subscribe_cb, NULL, TRUE);
	
	g_free(post);
	json_object_unref(obj);
	json_array_unref(interested);
}

static void
skypeweb_got_registration_token(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	gchar *registration_token;
	//gchar *endpointId;
	SkypeWebAccount *sa = user_data;
	
	if (url_text == NULL) {
		url_text = url_data->webdata;
		len = url_data->data_len;
	}
	
	registration_token = skypeweb_string_get_chunk(url_text, len, "Set-RegistrationToken: ", ";");
	//endpointId = skypeweb_string_get_chunk(url_text, len, "endpointId=", "\r\n");
	
	if (registration_token == NULL) {
		purple_connection_error (sa->pc,
								PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
								_("Failed getting Registration Token"));
		return;
	}
	purple_debug_info("skypeweb", "New RegistrationToken is %s\n", registration_token);
	
	sa->registration_token = registration_token;
	
	skypeweb_subscribe(sa);
}

void
skypeweb_get_registration_token(SkypeWebAccount *sa)
{
	const gchar *messages_url = "https://" SKYPEWEB_MESSAGES_HOST "/v1/users/ME/endpoints";
	gchar *request;
	gchar *curtime;
	gchar *response;
	PurpleUtilFetchUrlData *requestdata;
	
	g_free(sa->registration_token);
	sa->registration_token = NULL;
	
	curtime = g_strdup_printf("%d", (int) time(NULL));
	response = skypeweb_hmac_sha256(curtime);
	
	request = g_strdup_printf("POST /v1/users/ME/endpoints HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: */*\r\n"
			"LockAndKey: appId=" SKYPEWEB_LOCKANDKEY_APPID "; time=%s; lockAndKeyResponse=%s\r\n"
			"ClientInfo: os=Windows; osVer=8.1; proc=Win32; lcid=en-us; deviceType=1; country=n/a; clientName=swx-skype.com; clientVer=908/1.0.0.20\r\n"
			"Host: " SKYPEWEB_MESSAGES_HOST "\r\n"
			"Content-Type: application/json\r\n"
			"Authentication: skypetoken=%s\r\n"
			"Content-Length: 2\r\n\r\n{}",
			curtime, response, sa->skype_token);
	
	purple_debug_info("skypeweb", "reg token request is %s\n", request);
	
	requestdata = purple_util_fetch_url_request(sa->account, messages_url, TRUE, NULL, FALSE, request, TRUE, 524288, skypeweb_got_registration_token, sa);
	requestdata->num_times_redirected = 10; /* Prevent following redirects */

	g_free(request);
	g_free(curtime);
	g_free(response);
}





guint
skypeweb_send_typing(PurpleConnection *pc, const gchar *name, PurpleTypingState state)
{
	SkypeWebAccount *sa = pc->proto_data;
	gchar *post, *url;
	JsonObject *obj;
	
	url = g_strdup_printf("/v1/users/ME/conversations/8:%s/messages", purple_url_encode(name));
	
	obj = json_object_new();
	json_object_set_int_member(obj, "clientmessageid", time(NULL));
	json_object_set_string_member(obj, "content", "");
	json_object_set_string_member(obj, "messagetype", state == PURPLE_TYPING ? "Control/Typing" : "Control/ClearTyping");
	json_object_set_string_member(obj, "contenttype", "text");
	
	post = skypeweb_jsonobj_to_string(obj);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, SKYPEWEB_MESSAGES_HOST, url, post, NULL, NULL, TRUE);
	
	g_free(post);
	json_object_unref(obj);
	g_free(url);
	
	return 20;
}


static void
skypeweb_set_statusid(SkypeWebAccount *sa, const gchar *status)
{
	gchar *post;
	
	post = g_strdup_printf("{\"status\":\"%s\"}", status);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_MESSAGES_HOST, "/v1/users/ME/presenceDocs/messagingService", post, NULL, NULL, TRUE);
	
	g_free(post);
}

void
skypeweb_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	SkypeWebAccount *sa = pc->proto_data;
	
	skypeweb_set_statusid(sa, purple_status_get_id(status));
}

void
skypeweb_set_idle(PurpleConnection *pc, int time)
{
	SkypeWebAccount *sa = pc->proto_data;
	
	if (time < 30) {
		skypeweb_set_statusid(sa, "Online");
	} else {
		skypeweb_set_statusid(sa, "Idle");
	}
}






gint
skypeweb_send_im(PurpleConnection *pc, const gchar *who, const gchar *msg,
		PurpleMessageFlags flags)
{
	SkypeWebAccount *sa = pc->proto_data;
	gchar *post, *url;
	JsonObject *obj;
	
	url = g_strdup_printf("/v1/users/ME/conversations/8:%s/messages", purple_url_encode(who));
	
	obj = json_object_new();
	json_object_set_int_member(obj, "clientmessageid", g_random_int_range(100000, 999999));
	json_object_set_string_member(obj, "content", msg);
	json_object_set_string_member(obj, "messagetype", "RichText");
	json_object_set_string_member(obj, "contenttype", "text");
	
	post = skypeweb_jsonobj_to_string(obj);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, SKYPEWEB_MESSAGES_HOST, url, post, NULL, NULL, TRUE);
	
	g_free(post);
	json_object_unref(obj);
	g_free(url);
	
	return 1;
}