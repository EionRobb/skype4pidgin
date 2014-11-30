

#include "skypeweb_contacts.h"
#include "skypeweb_connection.h"
#include "skypeweb_util.h"

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
	if (sbuddy->avatar_url && sbuddy->avatar_url[0]) {
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
	guint index, length;
	GString *userids;
	gchar *search_term = user_data;

	PurpleNotifySearchResults *results;
	PurpleNotifySearchColumn *column;
	
	
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
	guint index;
	guint length;
	
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
	
		new_avatar = json_object_get_string_member(contact, "avatarUrl");
		if (new_avatar && (!sbuddy->avatar_url || !g_str_equal(sbuddy->avatar_url, new_avatar))) {
			g_free(sbuddy->avatar_url);
			sbuddy->avatar_url = g_strdup(new_avatar);			
			skypeweb_get_icon(buddy);
		}
		
		g_free(sbuddy->mood); sbuddy->mood = g_strdup(json_object_get_string_member(contact, "mood"));
		g_free(sbuddy->rich_mood); sbuddy->rich_mood = g_strdup(json_object_get_string_member(contact, "richMood"));
		g_free(sbuddy->country); sbuddy->country = g_strdup(json_object_get_string_member(contact, "country"));
		g_free(sbuddy->city); sbuddy->city = g_strdup(json_object_get_string_member(contact, "city"));
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
	
#define _SKYPE_USER_INFO(prop, key) if (prop && !json_object_get_null_member(userobj, (prop))) \
	purple_notify_user_info_add_pair_html(user_info, _(key), json_object_get_string_member(userobj, (prop)));
	
	_SKYPE_USER_INFO("firstname", "First name");
	_SKYPE_USER_INFO("lastname", "Last name");
	_SKYPE_USER_INFO("birthday", "Birthday");
	_SKYPE_USER_INFO("gender", "Gender");
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
		g_free(sbuddy->rich_mood); sbuddy->rich_mood = g_strdup(json_object_get_string_member(userobj, "richMood"));
		g_free(sbuddy->country); sbuddy->country = g_strdup(json_object_get_string_member(userobj, "country"));
		g_free(sbuddy->city); sbuddy->city = g_strdup(json_object_get_string_member(userobj, "city"));
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
	guint index, length;
	
	/* {
	"skypename": "eionrobb",
	"fullname": "Eion Robb",
	"authorized": true,
	"blocked": false,
	"display_name": "eionrobb",
	"pstn_number": null,
	"phone1": null,
	"phone1_label": null,
	"phone2": null,
	"phone2_label": null,
	"phone3": null,
	"phone3_label": null
} */

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
		
		purple_debug_info("skypeweb", "Found user %s\n", skypename);
		
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
		
		serv_got_alias(sa->pc, skypename, sbuddy->display_name); //purple_blist_server_alias_buddy(buddy, string_parts[3]);
		
		users_to_fetch = g_slist_prepend(users_to_fetch, (gpointer) skypename);
	}
	
	if (users_to_fetch)
	{
		skypeweb_get_friend_profiles(sa, users_to_fetch);
	}
	g_free(users_to_fetch);
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
	
	url = g_strdup_printf("/users/self/contacts/auth-request/%s/reject", purple_url_encode(buddy->name));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, NULL, NULL, NULL, TRUE);
	
	g_free(url);
}

static void
skypeweb_got_authrequests(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonArray *requests;
	guint index, length;
	
	requests = json_node_get_array(node);
	/* [{
		"event_time": "2014-10-21 18:53:53.763883",
		"sender": "gretike1984",
		"greeting": "Kedves Eion Robb! Szeretn\u00e9lek felvenni a partnerlist\u00e1mra."
	}] */
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