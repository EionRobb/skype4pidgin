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

#if !PURPLE_VERSION_CHECK(3, 0, 0)
#	include "ft.h"
#else
#	include "xfer.h"
#endif

// Check that the conversation hasn't been closed
static gboolean
purple_conversation_is_valid(PurpleConversation *conv)
{
	GList *convs = purple_get_conversations();
	
	return (g_list_find(convs, conv) != NULL);
}


typedef struct {
	PurpleXfer *xfer;
	JsonObject *info;
	gchar *from;
	gchar *url;
	gchar *id;
	SkypeWebAccount *sa;
	PurpleSslConnection *conn;
} SkypeWebFileTransfer;

static guint active_icon_downloads = 0;

static void
skypeweb_get_icon_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	PurpleBuddy *buddy = user_data;
	SkypeWebBuddy *sbuddy = purple_buddy_get_protocol_data(buddy);
	SkypeWebAccount *sa = sbuddy->sa;
	gchar *url = g_dataset_get_data(url_data, "url");

	sa->url_datas = g_slist_remove(sa->url_datas, url_data);
	
	active_icon_downloads--;
	
	if (!buddy) {
		g_dataset_destroy(url_data);
		return;
	}
	
	purple_buddy_icons_set_for_user(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy), g_memdup(url_text, len), len, url);
	
	g_dataset_destroy(url_data);
}

static void
skypeweb_get_icon_now(PurpleBuddy *buddy)
{
	SkypeWebBuddy *sbuddy;
	gchar *url;
	gpointer url_data;
	
	purple_debug_info("skypeweb", "getting new buddy icon for %s\n", purple_buddy_get_name(buddy));
	
	sbuddy = purple_buddy_get_protocol_data(buddy);
	if (sbuddy != NULL && sbuddy->avatar_url && sbuddy->avatar_url[0]) {
		url = g_strdup(sbuddy->avatar_url);
	} else {
		url = g_strdup_printf("https://api.skype.com/users/%s/profile/avatar", purple_url_encode(purple_buddy_get_name(buddy)));
	}
	
	url_data = skypeweb_fetch_url_request(sbuddy->sa, url, TRUE, NULL, FALSE, NULL, FALSE, 524288, skypeweb_get_icon_cb, buddy);
	g_dataset_set_data_full(url_data, "url", url, g_free);

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
	PurpleConnection *pc;
	SkypeWebAccount *sa;
	gint icon_id;
	gchar *msg_tmp;
	gchar *location;
	
	// Conversation could have been closed before we retrieved the image
	if (!purple_conversation_is_valid(conv)) {
		return;
	}
	
	pc = purple_conversation_get_connection(conv);
	sa = purple_connection_get_protocol_data(pc);
	sa->url_datas = g_slist_remove(sa->url_datas, url_data);

	location = skypeweb_string_get_chunk(url_text, len, "Location: https://", "/");
	if (location && *location) {
		skypeweb_download_uri_to_conv(sa, location, conv);
		g_free(location);
		return;
	}
	
	if (!url_text || !len || url_text[0] == '{' || url_text[0] == '<')
		return;
	
	if (error_message && *error_message)
		return;
	
	icon_id = purple_imgstore_add_with_id(g_memdup(url_text, len), len, NULL);
	
	msg_tmp = g_strdup_printf("<img id='%d'>", icon_id);
	purple_conversation_write(conv, conv->name, msg_tmp, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_IMAGES, time(NULL));
	g_free(msg_tmp);
	
	purple_imgstore_unref_by_id(icon_id);
}

void
skypeweb_download_uri_to_conv(SkypeWebAccount *sa, const gchar *uri, PurpleConversation *conv)
{
	gchar *headers, *url, *text;
	gpointer requestdata;
	PurpleHttpURL *httpurl;
	
	httpurl = purple_http_url_parse(uri);
	headers = g_strdup_printf("GET /%s HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: image/*\r\n"
			"Cookie: skypetoken_asm=%s\r\n"
			"Host: %s\r\n"
			"\r\n",
			purple_http_url_get_path(httpurl), sa->skype_token, 
			purple_http_url_get_host(httpurl));
	
	requestdata = skypeweb_fetch_url_request(sa, uri, TRUE, NULL, FALSE, headers, FALSE, -1, skypeweb_got_imagemessage, conv);

	skypeweb_url_prevent_follow_redirects(requestdata);
	
	g_free(headers);
	purple_http_url_free(httpurl);

	url = purple_strreplace(uri, "imgt1", "imgpsh_fullsize");
	text = g_strdup_printf("<a href=\"%s\">Click here to view full version</a>", url);
	purple_conversation_write(conv, conv->name, text, PURPLE_MESSAGE_SYSTEM, time(NULL));
}


static void
skypeweb_got_vm_file(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	PurpleXfer *xfer = user_data;
	SkypeWebAccount *sa = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));

	sa->url_datas = g_slist_remove(sa->url_datas, url_data);
	
	purple_xfer_write(xfer, (guchar *)url_text, len);
}

static void
skypeweb_init_vm_download(PurpleXfer *xfer)
{
	SkypeWebAccount *sa;
	JsonObject *file = xfer->data;
	gint64 fileSize;
	const gchar *url;

	fileSize = json_object_get_int_member(file, "fileSize");
	url = json_object_get_string_member(file, "url");
	
	purple_xfer_set_completed(xfer, FALSE);
	sa = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
	skypeweb_fetch_url_request(sa, url, TRUE, NULL, FALSE, NULL, FALSE, fileSize, skypeweb_got_vm_file, xfer);
	
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
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
	obj = json_node_get_object(node);
	
	files = json_object_get_array_member(obj, "files");
	file = json_array_get_object_element(files, 0);
	if (file != NULL) {
		status = json_object_get_string_member(file, "status");
		if (status && g_str_equal(status, "ok")) {
			assetId = json_object_get_string_member(obj, "assetId");
			fileSize = json_object_get_int_member(file, "fileSize");
			url = json_object_get_string_member(file, "url");
			(void) url;
			
			filename = g_strconcat(assetId, ".mp4", NULL);
			
			xfer = purple_xfer_new(sa->account, PURPLE_XFER_RECEIVE, conv->name);
			purple_xfer_set_size(xfer, fileSize);
			purple_xfer_set_filename(xfer, filename);
			json_object_ref(file);
			purple_xfer_set_protocol_data(xfer, file);
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
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
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
skypeweb_free_xfer(PurpleXfer *xfer)
{
	SkypeWebFileTransfer *swft;
	
	swft = purple_xfer_get_protocol_data(xfer);
	g_return_if_fail(swft != NULL);
	
	if (swft->info != NULL)
		json_object_unref(swft->info);
	if (swft->conn != NULL)
		purple_ssl_close(swft->conn);
	g_free(swft->url);
	g_free(swft->id);
	g_free(swft->from);
	g_free(swft);
	
	purple_xfer_set_protocol_data(xfer, NULL);
}

static void
skypeweb_got_file(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	SkypeWebFileTransfer *swft = user_data;
	PurpleXfer *xfer = swft->xfer;
	SkypeWebAccount *sa = swft->sa;

	sa->url_datas = g_slist_remove(sa->url_datas, url_data);
	
	if (error_message) {
		purple_xfer_error(purple_xfer_get_type(xfer), sa->account, swft->from, error_message);
		purple_xfer_cancel_local(xfer);
	} else {
		purple_xfer_write_file(xfer, (guchar *)url_text, len);
		purple_xfer_set_bytes_sent(xfer, len);
		purple_xfer_set_completed(xfer, TRUE);
	}
	
	//cleanup
	skypeweb_free_xfer(xfer);
}

static void
skypeweb_init_file_download(PurpleXfer *xfer)
{
	SkypeWebFileTransfer *swft;
	const gchar *view_location;
	gint64 content_full_length;
	PurpleHttpURL *httpurl;
	gchar *headers;
	
	swft = purple_xfer_get_protocol_data(xfer);
	
	view_location = json_object_get_string_member(swft->info, "view_location");
	content_full_length = json_object_get_int_member(swft->info, "content_full_length");
	
	purple_xfer_start(xfer, -1, NULL, 0);
	
	httpurl = purple_http_url_parse(view_location);
	headers = g_strdup_printf("GET /%s HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Cookie: skypetoken_asm=%s\r\n"
			"Host: %s\r\n"
			"\r\n",
			purple_http_url_get_path(httpurl), 
			swft->sa->skype_token, 
			purple_http_url_get_host(httpurl));
	
	skypeweb_fetch_url_request(swft->sa, view_location, TRUE, NULL, FALSE, headers, FALSE, content_full_length, skypeweb_got_file, swft);
	
	g_free(headers);
	purple_http_url_free(httpurl);
}

static void
skypeweb_got_file_info(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	JsonObject *obj;
	PurpleXfer *xfer;
	SkypeWebFileTransfer *swft = user_data;
	SkypeWebAccount *sa = swft->sa;
	JsonParser *parser;
	JsonNode *node;
	
	sa->url_datas = g_slist_remove(sa->url_datas, url_data);
	
	parser = json_parser_new();
	if (!json_parser_load_from_data(parser, url_text, len, NULL)) {
		g_free(swft->url);
		g_free(swft->from);
		g_free(swft);
		g_object_unref(parser);
		return;
	}
	
	node = json_parser_get_root(parser);
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT) {
		g_free(swft->url);
		g_free(swft->from);
		g_free(swft);
		g_object_unref(parser);
		return;
	}
	obj = json_node_get_object(node);
	
	/* 
	{
		"content_length": 40708,
		"content_full_length": 40708,
		"view_length": 40708,
		"content_state": "ready",
		"view_state": "ready",
		"view_location": "uri/views/original",
		"status_location": "uri/views/original/status",
		"scan": {
			"status": "passed"
		},
		"original_filename": "filename"
	} */
	purple_debug_info("skypeweb", "File info: %s\n", url_text);
	
	if (!json_object_has_member(obj, "content_state") || !g_str_equal(json_object_get_string_member(obj, "content_state"), "ready")) {
		skypeweb_present_uri_as_filetransfer(sa, json_object_get_string_member(obj, "status_location"), swft->from);
		g_free(swft->url);
		g_free(swft->from);
		g_free(swft);
		g_object_unref(parser);
		return;
	}
	
	json_object_ref(obj);
	swft->info = obj;
	
	xfer = purple_xfer_new(sa->account, PURPLE_XFER_RECEIVE, swft->from);
	purple_xfer_set_size(xfer, json_object_get_int_member(obj, "content_full_length"));
	purple_xfer_set_filename(xfer, json_object_get_string_member(obj, "original_filename"));
	purple_xfer_set_init_fnc(xfer, skypeweb_init_file_download);
	purple_xfer_set_cancel_recv_fnc(xfer, skypeweb_free_xfer);
	
	swft->xfer = xfer;
	purple_xfer_set_protocol_data(xfer, swft);
	
	purple_xfer_request(xfer);
	
	g_object_unref(parser);
}

void
skypeweb_present_uri_as_filetransfer(SkypeWebAccount *sa, const gchar *uri, const gchar *from)
{
	PurpleHttpURL *httpurl;
	const gchar *host;
	gchar *path;
	SkypeWebFileTransfer *swft;
	gchar *headers;
	
	
	httpurl = purple_http_url_parse(uri);
	host = purple_http_url_get_host(httpurl);
	
	if (g_str_has_suffix(uri, "/views/original/status")) {
		path = g_strconcat(purple_http_url_get_path(httpurl), NULL);
	} else {
		path = g_strconcat(purple_http_url_get_path(httpurl), "/views/original/status", NULL);
	}
	
	headers = g_strdup_printf("GET /%s HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Cookie: skypetoken_asm=%s\r\n"
			"Host: %s\r\n"
			"\r\n",
			path, 
			sa->skype_token, 
			host);
	
	swft = g_new0(SkypeWebFileTransfer, 1);
	swft->sa = sa;
	swft->url = g_strdup(uri);
	swft->from = g_strdup(from);
	
	skypeweb_fetch_url_request(sa, uri, TRUE, NULL, FALSE, headers, FALSE, -1, skypeweb_got_file_info, swft);
	
	g_free(path);
	g_free(headers);
	purple_http_url_free(httpurl);
}

static gssize
skypeweb_xfer_send_write(const guchar *buf, size_t len, PurpleXfer *xfer)
{
	SkypeWebFileTransfer *swft = purple_xfer_get_protocol_data(xfer);

	return purple_ssl_write(swft->conn, buf, len);
}

static void
skypeweb_read_stuff(gpointer user_data, PurpleSslConnection *ssl_connection, PurpleInputCondition cond)
{
	gchar buf[1024];
	
	//Flush the server buffer
	purple_ssl_read(ssl_connection, buf, 1024);
	//purple_debug_info("skypeweb", "Server file send response was %s\n", buf);
}

static void
skypeweb_xfer_send_connect_cb(gpointer user_data, PurpleSslConnection *ssl_connection, PurpleInputCondition cond)
{
	SkypeWebFileTransfer *swft = user_data;
	SkypeWebAccount *sa = swft->sa;
	PurpleXfer *xfer = swft->xfer;
	gchar *headers;
	
	headers = g_strdup_printf("PUT /v1/objects/%s/content/original HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Authorization: skype_token %s\r\n" //slightly different to normal!
			"Host: " SKYPEWEB_XFER_HOST "\r\n"
			"Content-Length: %" G_GSIZE_FORMAT "\r\n"
			"Content-Type: application/json\r\n"
			"\r\n",
			purple_url_encode(swft->id),
			sa->skype_token, 
			purple_xfer_get_size(xfer));
	
	purple_ssl_write(ssl_connection, headers, strlen(headers));
	
	swft->conn = ssl_connection;
	purple_ssl_input_add(swft->conn, skypeweb_read_stuff, swft);
	
	purple_xfer_ref(xfer);
	purple_xfer_start(xfer, ssl_connection->fd, NULL, 0);
	
	purple_xfer_prpl_ready(xfer);
	
	g_free(headers);
	
}

static gboolean poll_file_send_progress(gpointer user_data);

static void
got_file_send_progress(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	SkypeWebFileTransfer *swft = user_data;
	PurpleXfer *xfer = swft->xfer;
	SkypeWebAccount *sa = swft->sa;
	JsonParser *parser;
	JsonNode *node;
	JsonObject *obj;
	
	sa->url_datas = g_slist_remove(sa->url_datas, url_data);
	
	//{"content_length":0,"content_full_length":0,"view_length":0,"content_state":"no content","view_state":"none","view_location":"https://nus1-api.asm.skype.com/v1/objects/0-cus-d1-61121cfae8cf601944627a66afdb77ad/views/original","status_location":"https://nus1-api.asm.skype.com/v1/objects/0-cus-d1-61121cfae8cf601944627a66afdb77ad/views/original/status"}
	parser = json_parser_new();
	if (!json_parser_load_from_data(parser, url_text, len, NULL)) {
		//probably bad
		poll_file_send_progress(swft);
		return;
	}
	node = json_parser_get_root(parser);
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT) {
		//probably bad
		poll_file_send_progress(swft);
		return;
	}
	obj = json_node_get_object(node);
	
	
	if (json_object_has_member(obj, "status_location")) {
		g_free(swft->url);
		swft->url = g_strdup(json_object_get_string_member(obj, "status_location"));
	}
	
	if (json_object_has_member(obj, "content_state") && g_str_equal(json_object_get_string_member(obj, "content_state"), "ready")) {
		xmlnode *uriobject = xmlnode_new("URIObject");
		xmlnode *title = xmlnode_new_child(uriobject, "Title");
		xmlnode *description = xmlnode_new_child(uriobject, "Description");
		xmlnode *anchor = xmlnode_new_child(uriobject, "a");
		xmlnode *originalname = xmlnode_new_child(uriobject, "OriginalName");
		xmlnode *filesize = xmlnode_new_child(uriobject, "FileSize");
		gchar *message, *temp;
		//We finally did it!
		// May the pesants rejoyce
		purple_xfer_set_completed(xfer, TRUE);
		
		// Don't forget to let the other end know about it
		
		xmlnode_set_attrib(uriobject, "type", "File.1");
		temp = g_strconcat("https://" SKYPEWEB_XFER_HOST "/v1/objects/", purple_url_encode(swft->id), NULL);
		xmlnode_set_attrib(uriobject, "uri", temp);
		g_free(temp);
		temp = g_strconcat("https://" SKYPEWEB_XFER_HOST "/v1/objects/", purple_url_encode(swft->id), "/views/thumbnail", NULL);
		xmlnode_set_attrib(uriobject, "url_thumbnail", temp);
		g_free(temp);
		xmlnode_insert_data(title, purple_xfer_get_filename(xfer), -1);
		xmlnode_insert_data(description, "Description: ", -1);
		temp = g_strconcat("https://login.skype.com/login/sso?go=webclient.xmm&docid=", purple_url_encode(swft->id), NULL);
		xmlnode_set_attrib(anchor, "href", temp);
		xmlnode_insert_data(anchor, temp, -1);
		g_free(temp);
		xmlnode_set_attrib(originalname, "v", purple_xfer_get_filename(xfer));
		temp = g_strdup_printf("%" G_GSIZE_FORMAT, purple_xfer_get_size(xfer));
		xmlnode_set_attrib(filesize, "v", temp);
		g_free(temp);
		
		message = xmlnode_to_str(uriobject, NULL);
		skypeweb_send_im(sa->pc, swft->from, message, PURPLE_MESSAGE_SEND);
		g_free(message);
		
		skypeweb_free_xfer(xfer);
		purple_xfer_unref(xfer);
		
		xmlnode_free(uriobject);
		g_object_unref(parser);
		return;
	}
	
	
	g_object_unref(parser);
	
	// probably good
	poll_file_send_progress(swft);
}

static gboolean
poll_file_send_progress(gpointer user_data)
{
	SkypeWebFileTransfer *swft = user_data;
	SkypeWebAccount *sa = swft->sa;
	PurpleHttpURL *httpurl;
	const gchar *host, *path;
	gchar *headers;
	
	httpurl = purple_http_url_parse(swft->url);
	host = purple_http_url_get_host(httpurl);
	path = purple_http_url_get_path(httpurl);
	
	headers = g_strdup_printf("GET /%s HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Cookie: skypetoken_asm=%s\r\n"
			"Host: %s\r\n"
			"\r\n",
			path, 
			sa->skype_token, 
			host);
	
	skypeweb_fetch_url_request(sa, swft->url, TRUE, NULL, FALSE, headers, FALSE, -1, got_file_send_progress, swft);
	
	g_free(headers);
	purple_http_url_free(httpurl);
	
	return FALSE;
}

static void
skypeweb_xfer_send_end(PurpleXfer *xfer)
{
	SkypeWebFileTransfer *swft = purple_xfer_get_protocol_data(xfer);
	gchar buf[1024];
	
	//Flush the server buffer
	purple_ssl_read(swft->conn, buf, 1024);
	//purple_debug_info("skypeweb", "Server file send response was %s\n", buf);
	
	purple_ssl_close(swft->conn);
	swft->conn = NULL;
	
	//poll swft->url for progress
	purple_timeout_add_seconds(1, poll_file_send_progress, swft);
}

static void
skypeweb_got_object_for_file(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	SkypeWebFileTransfer *swft = user_data;
	SkypeWebAccount *sa = swft->sa;
	PurpleXfer *xfer = swft->xfer;
	JsonParser *parser;
	JsonNode *node;
	JsonObject *obj;

	sa->url_datas = g_slist_remove(sa->url_datas, url_data);
	
	//Get back {"id": "0-cus-d3-deadbeefdeadbeef012345678"}
	parser = json_parser_new();
	if (!json_parser_load_from_data(parser, url_text, len, NULL)) {
		g_free(swft->from);
		g_free(swft);
		g_object_unref(parser);
		return;
	}
	node = json_parser_get_root(parser);
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT) {
		g_free(swft->from);
		g_free(swft);
		g_object_unref(parser);
		purple_xfer_cancel_local(xfer);
		return;
	}
	obj = json_node_get_object(node);
	
	if (!json_object_has_member(obj, "id")) {
		g_free(swft->from);
		g_free(swft);
		g_object_unref(parser);
		purple_xfer_cancel_local(xfer);
		return;
	}
	
	swft->id = g_strdup(json_object_get_string_member(obj, "id"));
	swft->url = g_strconcat("https://" SKYPEWEB_XFER_HOST "/v1/objects/", purple_url_encode(swft->id), "/views/original/status", NULL);
	
	g_object_unref(parser);
	
	//Send the data
	
	//can't use fetch_url_request because it doesn't handle binary data
	//TODO make an error handler callback func
	purple_ssl_connect(sa->account, SKYPEWEB_XFER_HOST, 443, skypeweb_xfer_send_connect_cb, NULL, swft);
	
}

static void
skypeweb_xfer_send_init(PurpleXfer *xfer)
{
	PurpleConnection *pc = purple_account_get_connection(purple_xfer_get_account(xfer));
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *basename = g_path_get_basename(purple_xfer_get_local_filename(xfer));
	gchar *id, *post, *headers;
	SkypeWebFileTransfer *swft = purple_xfer_get_protocol_data(xfer);
	JsonObject *obj = json_object_new();
	JsonObject *permissions = json_object_new();
	JsonArray *userpermissions = json_array_new();
	
	purple_xfer_set_filename(xfer, basename);
	purple_xfer_ref(xfer);
	
	json_object_set_string_member(obj, "type", "sharing/file");
	json_object_set_string_member(obj, "filename", basename);
	
	id = g_strconcat(skypeweb_user_url_prefix(swft->from), swft->from, NULL);
	json_array_add_string_element(userpermissions, "read");
	json_object_set_array_member(permissions, id, userpermissions);
	json_object_set_object_member(obj, "permissions", permissions);
	
	post = skypeweb_jsonobj_to_string(obj);
	//POST to api.asm.skype.com  /v1/objects
	//{"type":"sharing/file","permissions":{"8:eionrobb":["read"]},"filename":"GiantLobsterMoose.txt"}
	
	headers = g_strdup_printf("POST /v1/objects HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Authorization: skype_token %s\r\n" //slightly different to normal!
			"Host: " SKYPEWEB_XFER_HOST "\r\n"
			"Content-Length: %" G_GSIZE_FORMAT "\r\n"
			"Content-Type: application/json\r\n"
			"\r\n%s",
			sa->skype_token, 
			strlen(post), post);
	
	skypeweb_fetch_url_request(sa, "https://" SKYPEWEB_XFER_HOST, TRUE, NULL, FALSE, headers, FALSE, -1, skypeweb_got_object_for_file, swft);
	
	g_free(post);
	json_object_unref(obj);
	g_free(id);
	g_free(basename);
}

PurpleXfer *
skypeweb_new_xfer(PurpleConnection *pc, const char *who)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	PurpleXfer *xfer;
	SkypeWebFileTransfer *swft;
	
	xfer = purple_xfer_new(sa->account, PURPLE_XFER_SEND, who);
	
	swft = g_new0(SkypeWebFileTransfer, 1);
	swft->sa = sa;
	swft->from = g_strdup(who);
	swft->xfer = xfer;
	purple_xfer_set_protocol_data(xfer, swft);
	
	purple_xfer_set_init_fnc(xfer, skypeweb_xfer_send_init);
	purple_xfer_set_write_fnc(xfer, skypeweb_xfer_send_write);
	purple_xfer_set_end_fnc(xfer, skypeweb_xfer_send_end);
	purple_xfer_set_request_denied_fnc(xfer, skypeweb_free_xfer);
	purple_xfer_set_cancel_send_fnc(xfer, skypeweb_free_xfer);
	
	return xfer;
}

void
skypeweb_send_file(PurpleConnection *pc, const gchar *who, const gchar *filename)
{
	PurpleXfer *xfer = skypeweb_new_xfer(pc, who);
	
	if (filename && *filename)
		purple_xfer_request_accepted(xfer, filename);
	else
		purple_xfer_request(xfer);
}

gboolean
skypeweb_can_receive_file(PurpleConnection *pc, const gchar *who)
{
	if (!who || g_str_equal(who, purple_account_get_username(purple_connection_get_account(pc))))
		return FALSE;
	
	return TRUE;
}


static void
skypeweb_got_self_details(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonObject *userobj;
	const gchar *old_alias;
	const gchar *displayname = NULL;
	const gchar *username;
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
	userobj = json_node_get_object(node);
	
	username = json_object_get_string_member(userobj, "username");
	g_free(sa->username); sa->username = g_strdup(username);
	purple_connection_set_display_name(sa->pc, sa->username);
	
	old_alias = purple_account_get_private_alias(sa->account);
	if (!old_alias || !*old_alias) {
		if (json_object_has_member(userobj, "displayname"))
			displayname = json_object_get_string_member(userobj, "displayname");
		if (!displayname || g_str_equal(displayname, username))
			displayname = json_object_get_string_member(userobj, "firstname");
	
		if (displayname)
			purple_account_set_private_alias(sa->account, displayname);
	}
	
	if (!PURPLE_CONNECTION_IS_CONNECTED(sa->pc)) {
		skypeweb_do_all_the_things(sa);
	}
}


void
skypeweb_get_self_details(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, "/users/self/profile", NULL, skypeweb_got_self_details, NULL, TRUE);
}









void
skypeweb_search_results_add_buddy(PurpleConnection *pc, GList *row, void *user_data)
{
	PurpleAccount *account = purple_connection_get_account(pc);

	if (!purple_blist_find_buddy(account, g_list_nth_data(row, 0)))
		purple_blist_request_add_buddy(account, g_list_nth_data(row, 0), "Skype", g_list_nth_data(row, 1));
}

void
skypeweb_search_users_text_cb(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonObject *response = NULL;
	JsonArray *resultsarray = NULL;
	gint index, length;
	gchar *search_term = user_data;

	PurpleNotifySearchResults *results;
	PurpleNotifySearchColumn *column;
	
	response = json_node_get_object(node);
	resultsarray = json_object_get_array_member(response, "results");
	length = json_array_get_length(resultsarray);
	
	if (length == 0)
	{
		gchar *primary_text = g_strdup_printf("Your search for the user \"%s\" returned no results", search_term);
		purple_notify_warning(sa->pc, "No users found", primary_text, "");
		g_free(primary_text);
		g_free(search_term);
		return;
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
		JsonObject *result = json_array_get_object_element(resultsarray, index);
		JsonObject *skypecontact = json_object_get_object_member(result, "nodeProfileData");
		
		/* the row in the search results table */
		/* prepend to it backwards then reverse to speed up adds */
		GList *row = NULL;

#define add_skypecontact_row(value) (\
		row = g_list_prepend(row, \
			!json_object_has_member(skypecontact, (value)) ? NULL : \
			g_strdup(json_object_get_string_member(skypecontact, (value)))\
		) \
)		
		add_skypecontact_row("skypeId");
		add_skypecontact_row("name");
		add_skypecontact_row("city");
		add_skypecontact_row("country");
		
		row = g_list_reverse(row);
		
		purple_notify_searchresults_row_add(results, row);
	}
	
	purple_notify_searchresults(sa->pc, NULL, search_term, NULL, results, NULL, NULL);
}

void
skypeweb_search_users_text(gpointer user_data, const gchar *text)
{
	SkypeWebAccount *sa = user_data;
	GString *url = g_string_new("/search/v1.1/namesearch/swx/?");
	
	g_string_append_printf(url, "searchstring=%s&", purple_url_encode(text));
	g_string_append(url, "requestId=1&");
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_GRAPH_HOST, url->str, NULL, skypeweb_search_users_text_cb, g_strdup(text), FALSE);
	
	g_string_free(url, TRUE);
}

void
skypeweb_search_users(PurplePluginAction *action)
{
	PurpleConnection *pc = (PurpleConnection *) action->context;
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	
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
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_ARRAY)
		return;
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
		sbuddy = purple_buddy_get_protocol_data(buddy);
		if (sbuddy == NULL) {
			sbuddy = g_new0(SkypeWebBuddy, 1);
			purple_buddy_set_protocol_data(buddy, sbuddy);
			sbuddy->skypename = g_strdup(username);
			sbuddy->sa = sa;
		}
		
		g_free(sbuddy->display_name); sbuddy->display_name = g_strdup(json_object_get_string_member(contact, "displayname"));
		purple_serv_got_alias(sa->pc, username, sbuddy->display_name);
		if (json_object_has_member(contact, "lastname")) {
			gchar *fullname = g_strconcat(json_object_get_string_member(contact, "firstname"), " ", json_object_get_string_member(contact, "lastname"), NULL);
			
			purple_blist_server_alias_buddy(buddy, fullname);
			
			g_free(fullname);
		} else {
			purple_blist_server_alias_buddy(buddy, json_object_get_string_member(contact, "firstname"));
		}
		
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
	
	if (node == NULL || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return;
	userobj = json_node_get_object(node);
	
	user_info = purple_notify_user_info_new();
	
#define _SKYPE_USER_INFO(prop, key) if (prop && json_object_has_member(userobj, (prop)) && !json_object_get_null_member(userobj, (prop))) \
	purple_notify_user_info_add_pair_html(user_info, _(key), json_object_get_string_member(userobj, (prop)));
	
	_SKYPE_USER_INFO("firstname", "First Name");
	_SKYPE_USER_INFO("lastname", "Last Name");
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
		sbuddy = purple_buddy_get_protocol_data(buddy);
		if (sbuddy == NULL) {
			sbuddy = g_new0(SkypeWebBuddy, 1);
			purple_buddy_set_protocol_data(buddy, sbuddy);
			sbuddy->skypename = g_strdup(username);
			sbuddy->sa = sa;
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
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
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
	JsonObject *obj;
	JsonArray *contacts;
	PurpleGroup *group = NULL;
	GSList *users_to_fetch = NULL;
	guint index, length;
	
	obj = json_node_get_object(node);
	contacts = json_object_get_array_member(obj, "contacts");
	length = json_array_get_length(contacts);
	
	for(index = 0; index < length; index++)
	{
		JsonObject *contact = json_array_get_object_element(contacts, index);
		const gchar *id = json_object_get_string_member(contact, "id");
		const gchar *display_name = json_object_get_string_member(contact, "display_name");
		const gchar *avatar_url = NULL;
		gboolean authorized = json_object_get_boolean_member(contact, "authorized");
		gboolean blocked = json_object_get_boolean_member(contact, "blocked");
		const gchar *type = json_object_get_string_member(contact, "type");
		const gchar *mood = json_object_get_string_member(contact, "mood");
		
		JsonObject *name = json_object_get_object_member(contact, "name");
		const gchar *firstname = json_object_get_string_member(name, "first");
		const gchar *surname = NULL;
		PurpleBuddy *buddy;
		
		//TODO make this work for "pstn"
		if (!g_str_equal(type, "skype") && !g_str_equal(type, "msn"))
			continue;
		
		if (json_object_has_member(contact, "suggested") && json_object_get_boolean_member(contact, "suggested") && !authorized) {
			// suggested buddies wtf? some kind of advertising?
			continue;
		}
		
		buddy = purple_find_buddy(sa->account, id);
		if (!buddy)
		{
			if (!group)
			{
				group = purple_blist_find_group("Skype");
				if (!group)
				{
					group = purple_group_new("Skype");
					purple_blist_add_group(group, NULL);
				}
			}
			buddy = purple_buddy_new(sa->account, id, display_name);
			purple_blist_add_buddy(buddy, NULL, group, NULL);
		}
		
		if (json_object_has_member(name, "surname"))
			surname = json_object_get_string_member(name, "surname");

		// try to free the sbuddy here. no-op if it's not set before, otherwise prevents a leak.
		skypeweb_buddy_free(buddy);
		
		SkypeWebBuddy *sbuddy = g_new0(SkypeWebBuddy, 1);
		sbuddy->skypename = g_strdup(id);
		sbuddy->sa = sa;
		sbuddy->fullname = g_strconcat(firstname, (surname ? " " : NULL), surname, NULL);
		sbuddy->display_name = g_strdup(display_name);
		sbuddy->authorized = authorized;
		sbuddy->blocked = blocked;
		sbuddy->avatar_url = g_strdup(purple_buddy_icons_get_checksum_for_user(buddy));
		sbuddy->mood = g_strdup(mood);
		
		sbuddy->buddy = buddy;
		purple_buddy_set_protocol_data(buddy, sbuddy);
		
		purple_serv_got_alias(sa->pc, id, sbuddy->display_name);
		purple_blist_server_alias_buddy(buddy, sbuddy->fullname);
		
		if (json_object_has_member(contact, "avatar_url")) {
			avatar_url = json_object_get_string_member(contact, "avatar_url");
			if (avatar_url && *avatar_url && (!sbuddy->avatar_url || !g_str_equal(sbuddy->avatar_url, avatar_url))) {
				g_free(sbuddy->avatar_url);
				sbuddy->avatar_url = g_strdup(avatar_url);			
				skypeweb_get_icon(buddy);
			}
		}
		
		if (blocked == TRUE) {
			purple_privacy_deny_add(sa->account, id, TRUE);
		} else {
			users_to_fetch = g_slist_prepend(users_to_fetch, sbuddy->skypename);
		}
	}
	
	if (users_to_fetch)
	{
		//skypeweb_get_friend_profiles(sa, users_to_fetch);
		skypeweb_subscribe_to_contact_status(sa, users_to_fetch);
		g_slist_free(users_to_fetch);
	}
}

void
skypeweb_get_friend_list(SkypeWebAccount *sa)
{
	gchar *url;
	
	if (!sa->username) {
		//purple_debug_error("skypeweb", "Can't fetch contacts yet, don't have local username");
		return;
	}
	
	url = g_strdup_printf("/contacts/v1/users/%s/contacts", purple_url_encode(sa->username));
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_NEW_CONTACTS_HOST, url, NULL, skypeweb_get_friend_list_cb, NULL, TRUE);
	
	g_free(url);
}



void
skypeweb_auth_accept_cb(gpointer sender)
{
	PurpleBuddy *buddy = sender;
	SkypeWebAccount *sa;
	gchar *url = NULL;
	GSList *users_to_fetch;
	gchar *buddy_name;
	
	sa = purple_connection_get_protocol_data(purple_account_get_connection(purple_buddy_get_account(buddy)));
	buddy_name = g_strdup(purple_buddy_get_name(buddy));
	
	url = g_strdup_printf("/contacts/v2/users/SELF/invites/%s/accept", purple_url_encode(buddy_name));
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_NEW_CONTACTS_HOST, url, NULL, NULL, NULL, TRUE);
	g_free(url);
	
	// Subscribe to status/message updates
	users_to_fetch = g_slist_prepend(NULL, buddy_name);
	skypeweb_subscribe_to_contact_status(sa, users_to_fetch);
	g_slist_free(users_to_fetch);
	g_free(buddy_name);
}

void
skypeweb_auth_reject_cb(gpointer sender)
{
	PurpleBuddy *buddy = sender;
	SkypeWebAccount *sa;
	gchar *url = NULL;
	
	sa = purple_connection_get_protocol_data(purple_account_get_connection(purple_buddy_get_account(buddy)));
	
	url = g_strdup_printf("/contacts/v2/users/SELF/invites/%s/decline", purple_url_encode(purple_buddy_get_name(buddy)));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_NEW_CONTACTS_HOST, url, NULL, NULL, NULL, TRUE);
	
	g_free(url);
}

static void
skypeweb_got_authrequests(SkypeWebAccount *sa, JsonNode *node, gpointer user_data)
{
	JsonObject *requests;
	JsonArray *invite_list;
	guint index, length;
	time_t latest_timestamp = 0;
	
	/* {
	"invite_list": [{
		"mri": "2:daniel.soderlund@lindab.com",
		"invites": [{
			"time": "2016-10-26T07:14:52.653Z"
		}],
		"displayname": "Söderlund, Daniel"
	}]
} */
	
	requests = json_node_get_object(node);
	invite_list = json_object_get_array_member(requests, "invite_list");
	length = json_array_get_length(invite_list);
	for(index = 0; index < length; index++)
	{
		JsonObject *invite = json_array_get_object_element(invite_list, index);
		JsonArray *invites = json_object_get_array_member(invite, "invites");
		const gchar *event_time_iso = json_object_get_string_member(json_array_get_object_element(invites, 0), "time");
		time_t event_timestamp = purple_str_to_time(event_time_iso, TRUE, NULL, NULL, NULL);
		const gchar *sender = json_object_get_string_member(invite, "mri");
		const gchar *greeting = json_object_get_string_member(invite, "greeting");
		const gchar *displayname = json_object_get_string_member(invite, "displayname");
		
		latest_timestamp = MAX(latest_timestamp, event_timestamp);
		if (sa->last_authrequest && event_timestamp <= sa->last_authrequest)
			continue;
		
		purple_account_request_authorization(
				sa->account, sender, NULL,
				displayname, greeting, FALSE,
				skypeweb_auth_accept_cb, skypeweb_auth_reject_cb, purple_buddy_new(sa->account, sender, displayname));
		
	}
	
	sa->last_authrequest = latest_timestamp;
}

gboolean
skypeweb_check_authrequests(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_NEW_CONTACTS_HOST, "/contacts/v2/users/SELF/invites", NULL, skypeweb_got_authrequests, NULL, TRUE);
	return TRUE;
}


void
skypeweb_add_buddy_with_invite(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char* message)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *url, *postdata;
	GSList *users_to_fetch;
	gchar *buddy_name;
	
	buddy_name = g_strdup(purple_buddy_get_name(buddy));
	url = g_strdup_printf("/users/self/contacts/auth-request/%s", purple_url_encode(buddy_name));
	postdata = g_strdup_printf("greeting=%s", message ? purple_url_encode(message) : "");
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, postdata, NULL, NULL, TRUE);
	
	g_free(postdata);
	g_free(url);
	
	// Subscribe to status/message updates
	users_to_fetch = g_slist_prepend(NULL, buddy_name);
	skypeweb_subscribe_to_contact_status(sa, users_to_fetch);
	g_slist_free(users_to_fetch);
	
	g_free(buddy_name);
}

void 
skypeweb_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	skypeweb_add_buddy_with_invite(gc, buddy, group, _("Please authorize me so I can add you to my buddy list."));
}

void
skypeweb_buddy_remove(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *url;
	
	url = g_strdup_printf("/users/self/contacts/%s", purple_url_encode(purple_buddy_get_name(buddy)));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_DELETE | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, NULL, NULL, NULL, TRUE);
	
	g_free(url);
	
	skypeweb_unsubscribe_from_contact_status(sa, purple_buddy_get_name(buddy));
}

void
skypeweb_buddy_block(PurpleConnection *pc, const char *name)
{
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
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
	SkypeWebAccount *sa = purple_connection_get_protocol_data(pc);
	gchar *url;
	
	url = g_strdup_printf("/users/self/contacts/%s/unblock", purple_url_encode(name));
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_PUT | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, url, "", NULL, NULL, TRUE);
	
	g_free(url);
}


void
skypeweb_set_mood_message(SkypeWebAccount *sa, const gchar *mood)
{
	JsonObject *obj, *payload;
	gchar *post;
	
	g_return_if_fail(mood);
	
	obj = json_object_new();
	payload = json_object_new();
	
	json_object_set_string_member(payload, "mood", mood);
	json_object_set_object_member(obj, "payload", payload);
	post = skypeweb_jsonobj_to_string(obj);
	
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_SSL, SKYPEWEB_CONTACTS_HOST, "/users/self/profile/partial", post, NULL, NULL, TRUE);
	
	g_free(post);
	json_object_unref(obj);
}
