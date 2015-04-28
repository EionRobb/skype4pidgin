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
 

#include "skypeweb_contacts.h"
#include "skypeweb_connection.h"
#include "skypeweb_messages.h"
#include "skypeweb_util.h"

#include <ft.h>

static guint active_icon_downloads = 0;

static void
skypeweb_get_icon_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	PurpleBuddy *buddy = user_data;
	SkypeWebBuddy *sbuddy;
	
	if (!buddy || !buddy->proto_data)
		return;
	
	sbuddy = buddy->proto_data;
	
	purple_buddy_icons_set_for_user(buddy->account, buddy->name, g_memdup(url_text, len), len, sbuddy->avatar_url);
	
	active_icon_downloads--;
}

static void
skypeweb_get_icon_now(PurpleBuddy *buddy)
{
	SkypeWebBuddy *sbuddy;
	gchar *url;
	
	purple_debug_info("skypeweb", "getting new buddy icon for %s\n", buddy->name);
	
	sbuddy = buddy->proto_data;
	if (sbuddy != NULL && sbuddy->avatar_url && sbuddy->avatar_url[0]) {
		url = g_strdup(sbuddy->avatar_url);
	} else {
		url = g_strdup_printf("https://api.skype.com/users/%s/profile/avatar", purple_url_encode(buddy->name));
	}
	
	purple_util_fetch_url_request(buddy->account, url, TRUE, NULL, FALSE, NULL, FALSE, 524288, skypeweb_get_icon_cb, buddy);

	g_free(url);

	active_icon_downloads++;
}

static gboolean
skypeweb_get_icon_queuepop(gpointer data)
{
	PurpleBuddy *buddy = data;
	
	// Only allow 4 simultaneous downloads
	if (active_icon_downloads > 4)
		return TRUE;
	
	skypeweb_get_icon_now(buddy);
	return FALSE;
}

void
skypeweb_get_icon(PurpleBuddy *buddy)
{
	if (!buddy) return;
	
	purple_timeout_add(100, skypeweb_get_icon_queuepop, (gpointer)buddy);
}


static void
skypeweb_got_imagemessage(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	PurpleConversation *conv = user_data;
	gint icon_id;
	gchar *msg_tmp;
	
	if (!url_text || !url_text[0] || url_text[0] == '{')
		return;
	
	if (error_message && *error_message)
		return;
	
	icon_id = purple_imgstore_add_with_id((gpointer)url_text, len, NULL);
	
	msg_tmp = g_strdup_printf("<img id='%d'>", icon_id);
	purple_conversation_write(conv, conv->name, msg_tmp, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_IMAGES, time(NULL));
	g_free(msg_tmp);
	
	purple_imgstore_unref_by_id(icon_id);
}

void
skypeweb_download_uri_to_conv(SkypeWebAccount *sa, const gchar *uri, PurpleConversation *conv)
{
	gchar *url;
	gchar *clean_uri;
	gchar *headers;
	
	clean_uri = purple_strreplace(uri, "/v1//", "/v1/");
	url = g_strconcat(clean_uri, "/views/imgo", NULL);
	
	headers = g_strdup_printf("GET %s HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: image/*\r\n"
			"Cookie: skypetoken_asm=%s\r\n"
			"Host: api.asm.skype.com\r\n"
			"\r\n\r\n",
			strchr(url, '/'), sa->skype_token);
	
	purple_util_fetch_url_request(sa->account, url, TRUE, NULL, FALSE, headers, FALSE, -1, skypeweb_got_imagemessage, conv);
	
	g_free(headers);
	g_free(url);
	g_free(clean_uri);
}


static void
skypeweb_got_vm_file(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	PurpleXfer *xfer = user_data;
	
	purple_xfer_write(xfer, (guchar *)url_text, len);
}

static void
skypeweb_init_vm_download(PurpleXfer *xfer)
{
	JsonObject *file = xfer->data;
	gint64 fileSize;
	const gchar *url;

	fileSize = json_object_get_int_member(file, "fileSize");
	url = json_object_get_string_member(file, "url");
	
	purple_xfer_set_completed(xfer, FALSE);
	purple_util_fetch_url_request(xfer->account, url, TRUE, NULL, FALSE, NULL, FALSE, fileSize, skypeweb_got_vm_file, xfer);
	
	json_object_unref(file);
}

static void
skypeweb_cancel_vm_download(PurpleXfer *xfer)
{
	JsonObject *file = xfer->data;
	json_object_unref(file);
}

static void
skypeweb_got_vm_download_info(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	PurpleConversation *conv = user_data;
	PurpleXfer *xfer;
	JsonObject *obj, *file;
	JsonArray *files;
	gint64 fileSize;
	const gchar *url, *assetId, *status;
	gchar *filename;
	
	obj = json_node_get_object(node);
	files = json_object_get_array_member(obj, "files");
	file = json_array_get_object_element(files, 0);
	if (file != NULL) {
		status = json_object_get_string_member(file, "status");
		if (status && g_str_equal(status, "ok")) {
			assetId = json_object_get_string_member(obj, "assetId");
			fileSize = json_object_get_int_member(file, "fileSize");
			url = json_object_get_string_member(file, "url");
			
			filename = g_strconcat(assetId, ".mp4", NULL);
			
			xfer = purple_xfer_new(sa->account, PURPLE_XFER_RECEIVE, conv->name);
			purple_xfer_set_size(xfer, fileSize);
			purple_xfer_set_filename(xfer, filename);
			json_object_ref(file);
			xfer->data = file;
			purple_xfer_set_init_fnc(xfer, skypeweb_init_vm_download);
			purple_xfer_set_cancel_recv_fnc(xfer, skypeweb_cancel_vm_download);
			purple_xfer_add(xfer);
			
			g_free(filename);
		} else if (status && g_str_equal(status, "running")) {
			//skypeweb_download_video_message(sa, sid??????, conv);
		}
	}
}

static void
skypeweb_got_vm_info(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	PurpleConversation *conv = user_data;
	JsonObject *obj, *response, *media_stream;
	const gchar *filename;
	
	obj = json_node_get_object(node);
	response = json_object_get_object_member(obj, "response");
	media_stream = json_object_get_object_member(response, "media_stream");
	filename = json_object_get_string_member(media_stream, "filename");
	
	if (filename != NULL) {
		// Need to keep retrying this url until it comes back with status:ok
		gchar *url = g_strdup_printf("/vod/api-create?assetId=%s&profile=mp4-vm", purple_url_encode(filename));
		skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, "media.vm.skype.com", url, NULL, skypeweb_got_vm_download_info, conv, TRUE);
		g_free(url);
	}

}

void
skypeweb_download_video_message(SkypeWebAccount *sa, const gchar *sid, PurpleConversation *conv)
{
	gchar *url, *username_encoded;
	
	username_encoded = g_strdup(purple_url_encode(sa->username));
	url = g_strdup_printf("/users/%s/video_mails/%s", username_encoded, purple_url_encode(sid));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_VIDEOMAIL_HOST, url, NULL, skypeweb_got_vm_info, conv, TRUE);
	
	g_free(url);
	g_free(username_encoded);
	
}


static void
skypeweb_got_self_details(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonObject *userobj;
	const gchar *old_alias;
	const gchar *displayname;
	const gchar *username;
	
	userobj = json_node_get_object(node);
	
	username = json_object_get_string_member(userobj, "username");
	g_free(sa->username); sa->username = g_strdup(username);
	
	old_alias = purple_account_get_alias(sa->account);
	if (!old_alias || !*old_alias) {
		displayname = json_object_get_string_member(userobj, "displayname");
		if (!displayname || g_str_equal(displayname, username))
			displayname = json_object_get_string_member(userobj, "firstname");
	
		if (displayname)
			purple_account_set_alias(sa->account, displayname);
	}
}


void
skypeweb_get_self_details(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, "/users/self/displayname", NULL, skypeweb_got_self_details, NULL, TRUE);
}









void
skypeweb_search_results_add_buddy(PurpleConnection *pc, GList *row, void *user_data)
{
	PurpleAccount *account = purple_connection_get_account(pc);

	if (!purple_find_buddy(account, g_list_nth_data(row, 0)))
		purple_blist_request_add_buddy(account, g_list_nth_data(row, 0), "Skype", g_list_nth_data(row, 1));
}

void
skypeweb_search_users_text_cb(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonArray *resultsarray = NULL;
	gint index, length;
	GString *userids;
	gchar *search_term = user_data;

	PurpleNotifySearchResults *results;
	PurpleNotifySearchColumn *column;
	
	resultsarray = json_node_get_array(node);
	length = json_array_get_length(resultsarray);
	
	if (length == 0)
	{
		gchar *primary_text = g_strdup_printf("Your search for the user \"%s\" returned no results", search_term);
		purple_notify_warning(sa->pc, "No users found", primary_text, "");
		g_free(primary_text);
		g_free(search_term);
		return;
	}
	
	userids = g_string_new("");
	
	resultsarray = json_node_get_array(node);
	for(index = 0; index < length; index++)
	{
		JsonObject *result = json_array_get_object_element(resultsarray, index);
		g_string_append_printf(userids, "%s,", json_object_get_string_member(result, "skypewebid"));
	}

	
	results = purple_notify_searchresults_new();
	if (results == NULL)
	{
		g_free(search_term);
		return;
	}
		
	/* columns: Friend ID, Name, Network */
	column = purple_notify_searchresults_column_new(_("Skype Name"));
	purple_notify_searchresults_column_add(results, column);
	column = purple_notify_searchresults_column_new(_("Display Name"));
	purple_notify_searchresults_column_add(results, column);
	column = purple_notify_searchresults_column_new(_("City"));
	purple_notify_searchresults_column_add(results, column);
	column = purple_notify_searchresults_column_new(_("Country"));
	purple_notify_searchresults_column_add(results, column);
	
	purple_notify_searchresults_button_add(results,
			PURPLE_NOTIFY_BUTTON_ADD,
			skypeweb_search_results_add_buddy);
	
	for(index = 0; index < length; index++)
	{
		JsonObject *contact = json_array_get_object_element(resultsarray, index);
		JsonObject *contactcards = json_object_get_object_member(contact, "ContactCards");
		JsonObject *skypecontact = json_object_get_object_member(contactcards, "Skype");
		JsonObject *currentlocation = json_object_get_object_member(contactcards, "CurrentLocation");
		
		/* the row in the search results table */
		/* prepend to it backwards then reverse to speed up adds */
		GList *row = NULL;
		
		row = g_list_prepend(row, g_strdup(json_object_get_string_member(skypecontact, "SkypeName")));
		row = g_list_prepend(row, g_strdup(json_object_get_string_member(skypecontact, "DisplayName")));
		row = g_list_prepend(row, g_strdup(json_object_get_string_member(currentlocation, "City")));
		row = g_list_prepend(row, g_strdup(json_object_get_string_member(currentlocation, "Country")));
		
		row = g_list_reverse(row);
		
		purple_notify_searchresults_row_add(results, row);
	}
	
	purple_notify_searchresults(sa->pc, NULL, search_term, NULL,
			results, NULL, NULL);
}

void
skypeweb_search_users_text(gpointer user_data, const gchar *text)
{
	SkypeWebAccount *sa = user_data;
	GString *url = g_string_new("/search/users/any?");
	
	g_string_append_printf(url, "keyWord=%s&", purple_url_encode(text));
	g_string_append(url, "contactTypes[]=skype&");
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url->str, NULL, skypeweb_search_users_text_cb, g_strdup(text), FALSE);
	
	g_string_free(url, TRUE);
}

void
skypeweb_search_users(PurplePluginAction *action)
{
	PurpleConnection *pc = (PurpleConnection *) action->context;
	SkypeWebAccount *sa = pc->proto_data;
	
	purple_request_input(pc, "Search for Skype Friends",
					   "Search for Skype Friends",
					   NULL,
					   NULL, FALSE, FALSE, NULL,
					   _("_Search"), G_CALLBACK(skypeweb_search_users_text),
					   _("_Cancel"), NULL,
					   purple_connection_get_account(pc), NULL, NULL,
					   sa);

}




static void
skypeweb_got_friend_profiles(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonArray *contacts;
	PurpleBuddy *buddy;
	SkypeWebBuddy *sbuddy;
	gint index, length;
	
	contacts = json_node_get_array(node);
	length = json_array_get_length(contacts);
	
	for(index = 0; index < length; index++)
	{
		JsonObject *contact = json_array_get_object_element(contacts, index);
		
		const gchar *username = json_object_get_string_member(contact, "username");
		const gchar *new_avatar;
		
		buddy = purple_find_buddy(sa->account, username);
		if (!buddy)
			continue;
		sbuddy = buddy->proto_data;
		if (sbuddy == NULL) {
			sbuddy = g_new0(SkypeWebBuddy, 1);
			buddy->proto_data = sbuddy;
			sbuddy->skypename = g_strdup(username);
		}
		
		g_free(sbuddy->display_name); sbuddy->display_name = g_strdup(json_object_get_string_member(contact, "displayname"));
		serv_got_alias(sa->pc, username, sbuddy->display_name);
		purple_blist_server_alias_buddy(buddy, json_object_get_string_member(contact, "firstname"));
		
		new_avatar = json_object_get_string_member(contact, "avatarUrl");
		if (new_avatar && *new_avatar && (!sbuddy->avatar_url || !g_str_equal(sbuddy->avatar_url, new_avatar))) {
			g_free(sbuddy->avatar_url);
			sbuddy->avatar_url = g_strdup(new_avatar);			
			skypeweb_get_icon(buddy);
		}
		
		g_free(sbuddy->mood); sbuddy->mood = g_strdup(json_object_get_string_member(contact, "mood"));
	}
}

void
skypeweb_get_friend_profiles(SkypeWebAccount *sa, GSList *contacts)
{
	const gchar *profiles_url = "/users/self/contacts/profiles";
	GString *postdata;
	GSList *cur = contacts;
	
	if (contacts == NULL)
		return;
	
	postdata = g_string_new("");
	
	do {
		g_string_append_printf(postdata, "&contacts[]=%s", purple_url_encode(cur->data));
	} while((cur = g_slist_next(cur)));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, profiles_url, postdata->str, skypeweb_got_friend_profiles, NULL, TRUE);
	
	g_string_free(postdata, TRUE);
}


static void
skypeweb_got_info(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	gchar *username = user_data;
	PurpleNotifyUserInfo *user_info;
	JsonObject *userobj;
	PurpleBuddy *buddy;
	SkypeWebBuddy *sbuddy;
	const gchar *new_avatar;
	
	userobj = json_node_get_object(node);
	
	user_info = purple_notify_user_info_new();
	
#define _SKYPE_USER_INFO(prop, key) if (prop && json_object_has_member(userobj, (prop)) && !json_object_get_null_member(userobj, (prop))) \
	purple_notify_user_info_add_pair_html(user_info, _(key), json_object_get_string_member(userobj, (prop)));
	
	_SKYPE_USER_INFO("firstname", "First name");
	_SKYPE_USER_INFO("lastname", "Last name");
	_SKYPE_USER_INFO("birthday", "Birthday");
	//_SKYPE_USER_INFO("gender", "Gender");
	if (json_object_has_member(userobj, "gender") && !json_object_get_null_member(userobj, "gender")) {
		const gchar *gender = json_object_get_string_member(userobj, "gender");
		const gchar *gender_output;
		if (*gender == '1') {
			gender_output = _("Male");
		} else if (*gender == '2') {
			gender_output = _("Female");
		} else {
			gender_output = _("Unknown");
		}
		purple_notify_user_info_add_pair_html(user_info, _("Gender"), gender_output);
	}
	_SKYPE_USER_INFO("language", "Language");
	_SKYPE_USER_INFO("country", "Country");
	_SKYPE_USER_INFO("province", "Province");
	_SKYPE_USER_INFO("city", "City");
	_SKYPE_USER_INFO("homepage", "Homepage");
	_SKYPE_USER_INFO("about", "About");
	_SKYPE_USER_INFO("jobtitle", "Job Title");
	_SKYPE_USER_INFO("phoneMobile", "Phone - Mobile");
	_SKYPE_USER_INFO("phoneHome", "Phone - Home");
	_SKYPE_USER_INFO("phoneOffice", "Phone - Office");
	//_SKYPE_USER_INFO("mood", "Mood");
	//_SKYPE_USER_INFO("richMood", "Mood");
	//_SKYPE_USER_INFO("avatarUrl", "Avatar");
	
	buddy = purple_find_buddy(sa->account, username);
	if (buddy) {
		sbuddy = buddy->proto_data;
		if (sbuddy == NULL) {
			sbuddy = g_new0(SkypeWebBuddy, 1);
			buddy->proto_data = sbuddy;
			sbuddy->skypename = g_strdup(username);
		}
		
		new_avatar = json_object_get_string_member(userobj, "avatarUrl");
		if (new_avatar && (!sbuddy->avatar_url || !g_str_equal(sbuddy->avatar_url, new_avatar))) {
			g_free(sbuddy->avatar_url);
			sbuddy->avatar_url = g_strdup(new_avatar);			
			skypeweb_get_icon(buddy);
		}
		
		g_free(sbuddy->mood); sbuddy->mood = g_strdup(json_object_get_string_member(userobj, "mood"));
	}
	
	purple_notify_userinfo(sa->pc, username, user_info, NULL, NULL);
	
	g_free(username);
}

void
skypeweb_get_info(PurpleConnection *pc, const gchar *username)
{
	SkypeWebAccount *sa = pc->proto_data;
	gchar *url;
	
	url = g_strdup_printf("/users/%s/profile", purple_url_encode(username));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, NULL, skypeweb_got_info, g_strdup(username), TRUE);
	
	g_free(url);
}

void
skypeweb_get_friend_profile(SkypeWebAccount *sa, const gchar *who)
{
	GSList *contacts = NULL;
	gchar *whodup;
	
	g_return_if_fail(sa && who && *who);
	
	whodup = g_strdup(who);
	contacts = g_slist_prepend(contacts, whodup);
	
	skypeweb_get_friend_profiles(sa, contacts);
	
	g_free(contacts);
	g_free(whodup);
}

static void
skypeweb_get_friend_list_cb(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonArray *friends;
	PurpleGroup *group = NULL;
	GSList *users_to_fetch = NULL;
	gint index, length;
	
	friends = json_node_get_array(node);
	length = json_array_get_length(friends);
	
	for(index = 0; index < length; index++)
	{
		JsonObject *friend = json_array_get_object_element(friends, index);
		const gchar *skypename = json_object_get_string_member(friend, "skypename");
		const gchar *fullname = json_object_get_string_member(friend, "fullname");
		const gchar *display_name = json_object_get_string_member(friend, "display_name");
		gboolean authorized = json_object_get_boolean_member(friend, "authorized");
		gboolean blocked = json_object_get_boolean_member(friend, "blocked");
		PurpleBuddy *buddy;
		
		buddy = purple_find_buddy(sa->account, skypename);
		if (!buddy)
		{
			if (!group)
			{
				group = purple_find_group("Skype");
				if (!group)
				{
					group = purple_group_new("Skype");
					purple_blist_add_group(group, NULL);
				}
			}
			buddy = purple_buddy_new(sa->account, skypename, display_name);
			purple_blist_add_buddy(buddy, NULL, group, NULL);
		}
		
		SkypeWebBuddy *sbuddy = g_new0(SkypeWebBuddy, 1);
		sbuddy->skypename = g_strdup(skypename);
		sbuddy->fullname = g_strdup(fullname);
		sbuddy->display_name = g_strdup(display_name);
		sbuddy->authorized = authorized;
		sbuddy->blocked = blocked;
		
		sbuddy->buddy = buddy;
		buddy->proto_data = sbuddy;
		
		serv_got_alias(sa->pc, skypename, sbuddy->display_name);
		purple_blist_server_alias_buddy(buddy, fullname);
		
		users_to_fetch = g_slist_prepend(users_to_fetch, sbuddy->skypename);
	}
	
	if (users_to_fetch)
	{
		skypeweb_get_friend_profiles(sa, users_to_fetch);
		skypeweb_subscribe_to_contact_status(sa, users_to_fetch);
		g_slist_free(users_to_fetch);
	}
}

void
skypeweb_get_friend_list(SkypeWebAccount *sa)
{	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, "/users/self/contacts", NULL, skypeweb_get_friend_list_cb, NULL, TRUE);
}



void
skypeweb_auth_accept_cb(gpointer sender)
{
	PurpleBuddy *buddy = sender;
	SkypeWebAccount *sa;
	gchar *url = NULL;
	
	sa = buddy->account->gc->proto_data;
	
	url = g_strdup_printf("/users/self/contacts/auth-request/%s/accept", purple_url_encode(buddy->name));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, NULL, NULL, NULL, TRUE);
	
	g_free(url);
}

void
skypeweb_auth_reject_cb(gpointer sender)
{
	PurpleBuddy *buddy = sender;
	SkypeWebAccount *sa;
	gchar *url = NULL;
	
	sa = buddy->account->gc->proto_data;
	
	url = g_strdup_printf("/users/self/contacts/auth-request/%s/decline", purple_url_encode(buddy->name));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, NULL, NULL, NULL, TRUE);
	
	g_free(url);
}

static void
skypeweb_got_authrequests(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonArray *requests;
	gint index, length;
	
	requests = json_node_get_array(node);
	length = json_array_get_length(requests);
	for(index = 0; index < length; index++)
	{
		JsonObject *request = json_array_get_object_element(requests, index);
		//const gchar *event_time = json_object_get_string_member(request, "event_time");
		const gchar *sender = json_object_get_string_member(request, "sender");
		const gchar *greeting = json_object_get_string_member(request, "greeting");
		
		purple_account_request_authorization(
				sa->account, sender, NULL,
				NULL, greeting, FALSE,
				skypeweb_auth_accept_cb, skypeweb_auth_reject_cb, purple_buddy_new(sa->account, sender, NULL));
					
	}
}

gboolean
skypeweb_check_authrequests(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, "/users/self/contacts/auth-request", NULL, skypeweb_got_authrequests, NULL, TRUE);
	return TRUE;
}


void
skypeweb_add_buddy_with_invite(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char* message)
{
	SkypeWebAccount *sa = pc->proto_data;
	gchar *url, *postdata;
	
	url = g_strdup_printf("/users/self/contacts/auth-request/%s", purple_url_encode(buddy->name));
	postdata = g_strdup_printf("greeting=%s", message ? purple_url_encode(message) : "");
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, postdata, NULL, NULL, TRUE);
	
	g_free(postdata);
	g_free(url);
}

void 
skypeweb_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	skypeweb_add_buddy_with_invite(gc, buddy, group, _("Please authorize me so I can add you to my buddy list."));
}

void
skypeweb_buddy_remove(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	//SkypeWebAccount *sa = pc->proto_data;
	
}

void
skypeweb_buddy_block(PurpleConnection *pc, const char *name)
{
	SkypeWebAccount *sa = pc->proto_data;
	gchar *url, *postdata;
	
	url = g_strdup_printf("/users/self/contacts/%s/block", purple_url_encode(name));
	postdata = g_strdup_printf("reporterIp=127.0.0.1&uiVersion=" SKYPEWEB_CLIENTINFO_VERSION "/" SKYPEWEB_CLIENTINFO_NAME);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, postdata, NULL, NULL, TRUE);
	
	g_free(postdata);
	g_free(url);
}

void
skypeweb_buddy_unblock(PurpleConnection *pc, const char *name)
{
	SkypeWebAccount *sa = pc->proto_data;
	gchar *url;
	
	url = g_strdup_printf("/users/self/contacts/%s/unblock", purple_url_encode(name));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, "", NULL, NULL, TRUE);
	
	g_free(url);
}