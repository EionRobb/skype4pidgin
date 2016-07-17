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
 
#include "skypeweb_connection.h"

#if !PURPLE_VERSION_CHECK(3, 0, 0)
	#define purple_connection_error purple_connection_error_reason
	#define purple_proxy_info_get_proxy_type purple_proxy_info_get_type
	#define purple_account_is_disconnecting(account) (account->disconnecting)
#endif

#if !GLIB_CHECK_VERSION (2, 22, 0)
#define g_hostname_is_ip_address(hostname) (g_ascii_isdigit(hostname[0]) && g_strstr_len(hostname, 4, "."))
#endif

static void skypeweb_attempt_connection(SkypeWebConnection *);
static void skypeweb_next_connection(SkypeWebAccount *sa);

#include <zlib.h>

static gchar *skypeweb_gunzip(const guchar *gzip_data, gssize *len_ptr)
{
	gsize gzip_data_len	= *len_ptr;
	z_stream zstr;
	int gzip_err = 0;
	gchar *data_buffer;
	gulong gzip_len = G_MAXUINT16;
	GString *output_string = NULL;

	data_buffer = g_new0(gchar, gzip_len);

	zstr.next_in = NULL;
	zstr.avail_in = 0;
	zstr.zalloc = Z_NULL;
	zstr.zfree = Z_NULL;
	zstr.opaque = 0;
	gzip_err = inflateInit2(&zstr, MAX_WBITS+32);
	if (gzip_err != Z_OK)
	{
		g_free(data_buffer);
		purple_debug_error("skypeweb", "no built-in gzip support in zlib\n");
		return NULL;
	}
	
	zstr.next_in = (Bytef *)gzip_data;
	zstr.avail_in = gzip_data_len;
	
	zstr.next_out = (Bytef *)data_buffer;
	zstr.avail_out = gzip_len;
	
	gzip_err = inflate(&zstr, Z_SYNC_FLUSH);

	if (gzip_err == Z_DATA_ERROR)
	{
		inflateEnd(&zstr);
		gzip_err = inflateInit2(&zstr, -MAX_WBITS);
		if (gzip_err != Z_OK)
		{
			g_free(data_buffer);
			purple_debug_error("skypeweb", "Cannot decode gzip header\n");
			return NULL;
		}
		zstr.next_in = (Bytef *)gzip_data;
		zstr.avail_in = gzip_data_len;
		zstr.next_out = (Bytef *)data_buffer;
		zstr.avail_out = gzip_len;
		gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
	}
	output_string = g_string_new("");
	while (gzip_err == Z_OK)
	{
		//append data to buffer
		output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
		//reset buffer pointer
		zstr.next_out = (Bytef *)data_buffer;
		zstr.avail_out = gzip_len;
		gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
	}
	if (gzip_err == Z_STREAM_END)
	{
		output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
	} else {
		purple_debug_error("skypeweb", "gzip inflate error\n");
	}
	inflateEnd(&zstr);

	g_free(data_buffer);	

	if (len_ptr)
		*len_ptr = output_string->len;

	return g_string_free(output_string, FALSE);
}

void
skypeweb_connection_close(SkypeWebConnection *skypewebcon)
{
	skypewebcon->sa->conns = g_slist_remove(skypewebcon->sa->conns, skypewebcon);
	
	if (skypewebcon->connect_data != NULL) {
		purple_proxy_connect_cancel(skypewebcon->connect_data);
		skypewebcon->connect_data = NULL;
	}

	if (skypewebcon->ssl_conn != NULL) {
		purple_ssl_close(skypewebcon->ssl_conn);
		skypewebcon->ssl_conn = NULL;
	}

	if (skypewebcon->fd >= 0) {
		close(skypewebcon->fd);
		skypewebcon->fd = -1;
	}

	if (skypewebcon->input_watcher > 0) {
		purple_input_remove(skypewebcon->input_watcher);
		skypewebcon->input_watcher = 0;
	}
	
	purple_timeout_remove(skypewebcon->timeout_watcher);
	
	g_free(skypewebcon->rx_buf);
	skypewebcon->rx_buf = NULL;
	skypewebcon->rx_len = 0;
}

void skypeweb_connection_destroy(SkypeWebConnection *skypewebcon)
{
	skypeweb_connection_close(skypewebcon);
	
	if (skypewebcon->request != NULL)
		g_string_free(skypewebcon->request, TRUE);
	
	g_free(skypewebcon->url);
	g_free(skypewebcon->hostname);
	g_free(skypewebcon);
}

void
skypeweb_update_cookies(SkypeWebAccount *sa, const gchar *headers)
{
	const gchar *cookie_start;
	const gchar *cookie_end;
	gchar *cookie_name;
	gchar *cookie_value;
	int header_len;

	g_return_if_fail(headers != NULL);

	header_len = strlen(headers);

	/* look for the next "Set-Cookie: " */
	/* grab the data up until ';' */
	cookie_start = headers;
	while ((cookie_start = strstr(cookie_start, "\r\nSet-Cookie: ")) &&
			(cookie_start - headers) < header_len)
	{
		cookie_start += 14;
		cookie_end = strchr(cookie_start, '=');
		cookie_name = g_strndup(cookie_start, cookie_end-cookie_start);
		cookie_start = cookie_end + 1;
		cookie_end = strchr(cookie_start, ';');
		cookie_value= g_strndup(cookie_start, cookie_end-cookie_start);
		cookie_start = cookie_end;

		g_hash_table_replace(sa->cookie_table, cookie_name,
				cookie_value);
	}
}

static void skypeweb_connection_process_data(SkypeWebConnection *skypewebcon)
{
	gssize len;
	gchar *tmp;

	len = skypewebcon->rx_len;
	tmp = g_strstr_len(skypewebcon->rx_buf, len, "\r\n\r\n");
	if (tmp == NULL) {
		/* This is a corner case that occurs when the connection is
		 * prematurely closed either on the client or the server.
		 * This can either be no data at all or a partial set of
		 * headers.  We pass along the data to be good, but don't
		 * do any fancy massaging.  In all likelihood the result will
		 * be tossed by the connection callback func anyways
		 */
		tmp = g_strndup(skypewebcon->rx_buf, len);
	} else {
		tmp += 4;
		len -= g_strstr_len(skypewebcon->rx_buf, len, "\r\n\r\n") -
				skypewebcon->rx_buf + 4;
		tmp = g_memdup(tmp, len + 1);
		tmp[len] = '\0';
		skypewebcon->rx_buf[skypewebcon->rx_len - len] = '\0';
		skypeweb_update_cookies(skypewebcon->sa, skypewebcon->rx_buf);

		if (strstr(skypewebcon->rx_buf, "Content-Encoding: gzip"))
		{
			/* we've received compressed gzip data, decompress */
			gchar *gunzipped;
			gunzipped = skypeweb_gunzip((const guchar *)tmp, &len);
			g_free(tmp);
			tmp = gunzipped;
		}
	}

	purple_debug_misc("skypeweb", "Got response: %s\n", skypewebcon->rx_buf);
	g_free(skypewebcon->rx_buf);
	skypewebcon->rx_buf = NULL;

	if (skypewebcon->callback != NULL) {
		if (!len)
		{
			purple_debug_info("skypeweb", "No data in response\n");
			skypewebcon->callback(skypewebcon->sa, NULL, skypewebcon->user_data);
		} else {
			JsonParser *parser = json_parser_new();
			if (!json_parser_load_from_data(parser, tmp, len, NULL))
			{
				if (skypewebcon->error_callback != NULL) {
					skypewebcon->error_callback(skypewebcon->sa, tmp, len, skypewebcon->user_data);
				} else {
					purple_debug_error("skypeweb", "Error parsing response: %s\n", tmp);
				}
			} else {
				JsonNode *root = json_parser_get_root(parser);
				
				purple_debug_info("skypeweb", "executing callback for %s\n", skypewebcon->url);
				skypewebcon->callback(skypewebcon->sa, root, skypewebcon->user_data);
			}
			g_object_unref(parser);
		}
	}

	g_free(tmp);
}

static void skypeweb_fatal_connection_cb(SkypeWebConnection *skypewebcon)
{
	PurpleConnection *pc = skypewebcon->sa->pc;

	purple_debug_error("skypeweb", "fatal connection error\n");

	skypeweb_connection_destroy(skypewebcon);

	/* We died.  Do not pass Go.  Do not collect $200 */
	/* In all seriousness, don't attempt to call the normal callback here.
	 * That may lead to the wrong error message being displayed */
	purple_connection_error(pc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Server closed the connection."));

}

static void skypeweb_post_or_get_readdata_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	SkypeWebConnection *skypewebcon;
	SkypeWebAccount *sa;
	gchar buf[4096];
	gssize len;

	skypewebcon = data;
	sa = skypewebcon->sa;

	if (skypewebcon->method & SKYPEWEB_METHOD_SSL) {
		len = purple_ssl_read(skypewebcon->ssl_conn,
				buf, sizeof(buf) - 1);
	} else {
		len = recv(skypewebcon->fd, buf, sizeof(buf) - 1, 0);
	}

	if (len < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			/* Try again later */
			return;
		}

		if (skypewebcon->method & SKYPEWEB_METHOD_SSL && skypewebcon->rx_len > 0) {
			/*
			 * This is a slightly hacky workaround for a bug in either
			 * GNU TLS or in the SSL implementation on skypeweb's web
			 * servers.  The sequence of events is:
			 * 1. We attempt to read the first time and successfully read
			 *    the server's response.
			 * 2. We attempt to read a second time and libpurple's call
			 *    to gnutls_record_recv() returns the error
			 *    GNUTLS_E_UNEXPECTED_PACKET_LENGTH, or
			 *    "A TLS packet with unexpected length was received."
			 *
			 * Normally the server would have closed the connection
			 * cleanly and this second read() request would have returned
			 * 0.  Or maybe it's normal for SSL connections to be severed
			 * in this manner?  In any case, this differs from the behavior
			 * of the standard recv() system call.
			 */
			purple_debug_warning("skypeweb",
				"ssl error, but data received.  attempting to continue\n");
		} else {
			/* Try resend the request */
			skypewebcon->retry_count++;
			if (skypewebcon->retry_count < 3) {
				skypeweb_connection_close(skypewebcon);
				skypewebcon->request_time = time(NULL);
				
				g_queue_push_head(sa->waiting_conns, skypewebcon);
				skypeweb_next_connection(sa);
			} else {
				skypeweb_fatal_connection_cb(skypewebcon);
			}
			return;
		}
	}

	if (len > 0)
	{
		buf[len] = '\0';

		skypewebcon->rx_buf = g_realloc(skypewebcon->rx_buf,
				skypewebcon->rx_len + len + 1);
		memcpy(skypewebcon->rx_buf + skypewebcon->rx_len, buf, len + 1);
		skypewebcon->rx_len += len;

		/* Wait for more data before processing */
		return;
	}

	/* The server closed the connection, let's parse the data */
	skypeweb_connection_process_data(skypewebcon);

	skypeweb_connection_destroy(skypewebcon);
	
	skypeweb_next_connection(sa);
}

static void skypeweb_post_or_get_ssl_readdata_cb (gpointer data,
		PurpleSslConnection *ssl, PurpleInputCondition cond)
{
	skypeweb_post_or_get_readdata_cb(data, -1, cond);
}

static void skypeweb_post_or_get_connect_cb(gpointer data, gint source,
		const gchar *error_message)
{
	SkypeWebConnection *skypewebcon;
	gssize len;

	skypewebcon = data;
	skypewebcon->connect_data = NULL;

	if (error_message)
	{
		purple_debug_error("skypeweb", "post_or_get_connect failure to %s\n", skypewebcon->url);
		purple_debug_error("skypeweb", "post_or_get_connect_cb %s\n",
				error_message);
		skypeweb_fatal_connection_cb(skypewebcon);
		return;
	}

	skypewebcon->fd = source;

	/* TODO: Check the return value of write() */
	len = write(skypewebcon->fd, skypewebcon->request->str,
			skypewebcon->request->len);
	(void) len;
	skypewebcon->input_watcher = purple_input_add(skypewebcon->fd,
			PURPLE_INPUT_READ,
			skypeweb_post_or_get_readdata_cb, skypewebcon);
}

static void skypeweb_post_or_get_ssl_connect_cb(gpointer data,
		PurpleSslConnection *ssl, PurpleInputCondition cond)
{
	SkypeWebConnection *skypewebcon;
	gssize len;

	skypewebcon = data;

	purple_debug_info("skypeweb", "post_or_get_ssl_connect_cb\n");

	/* TODO: Check the return value of write() */
	len = purple_ssl_write(skypewebcon->ssl_conn,
			skypewebcon->request->str, skypewebcon->request->len);
	(void) len;
	purple_ssl_input_add(skypewebcon->ssl_conn,
			skypeweb_post_or_get_ssl_readdata_cb, skypewebcon);
}

static void skypeweb_host_lookup_cb(GSList *hosts, gpointer data,
		const char *error_message)
{
	GSList *host_lookup_list;
	struct sockaddr_in *addr;
	gchar *hostname;
	gchar *ip_address;
	SkypeWebAccount *sa;
	PurpleDnsQueryData *query;

	/* Extract variables */
	host_lookup_list = data;

	sa = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);
	hostname = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);
	query = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);

	/* The callback has executed, so we no longer need to keep track of
	 * the original query.  This always needs to run when the cb is 
	 * executed. */
	sa->dns_queries = g_slist_remove(sa->dns_queries, query);

	/* Any problems, capt'n? */
	if (error_message != NULL)
	{
		purple_debug_warning("skypeweb",
				"Error doing host lookup: %s\n", error_message);
		return;
	}

	if (hosts == NULL)
	{
		purple_debug_warning("skypeweb",
				"Could not resolve host name\n");
		return;
	}

	/* Discard the length... */
	hosts = g_slist_delete_link(hosts, hosts);
	/* Copy the address then free it... */
	addr = hosts->data;
	ip_address = g_strdup(inet_ntoa(addr->sin_addr));
	g_free(addr);
	hosts = g_slist_delete_link(hosts, hosts);

	/*
	 * DNS lookups can return a list of IP addresses, but we only cache
	 * the first one.  So free the rest.
	 */
	while (hosts != NULL)
	{
		/* Discard the length... */
		hosts = g_slist_delete_link(hosts, hosts);
		/* Free the address... */
		g_free(hosts->data);
		hosts = g_slist_delete_link(hosts, hosts);
	}

	g_hash_table_insert(sa->hostname_ip_cache, hostname, ip_address);
}

static void skypeweb_cookie_foreach_cb(gchar *cookie_name,
		gchar *cookie_value, GString *str)
{
	/* TODO: Need to escape name and value? */
	g_string_append_printf(str, "%s=%s;", cookie_name, cookie_value);
}

/**
 * Serialize the sa->cookie_table hash table to a string.
 */
gchar *skypeweb_cookies_to_string(SkypeWebAccount *sa)
{
	GString *str;

	str = g_string_new(NULL);

	g_hash_table_foreach(sa->cookie_table,
			(GHFunc)skypeweb_cookie_foreach_cb, str);

	return g_string_free(str, FALSE);
}

static void skypeweb_ssl_connection_error(PurpleSslConnection *ssl,
		PurpleSslErrorType errortype, gpointer data)
{
	SkypeWebConnection *skypewebcon = data;
	SkypeWebAccount *sa = skypewebcon->sa;
	PurpleConnection *pc = sa->pc;
	
	skypewebcon->ssl_conn = NULL;
	
	/* Try resend the request */
	skypewebcon->retry_count++;
	if (skypewebcon->retry_count < 3) {
		skypeweb_connection_close(skypewebcon);
		skypewebcon->request_time = time(NULL);
		
		g_queue_push_head(sa->waiting_conns, skypewebcon);
		skypeweb_next_connection(sa);
	} else {
		skypeweb_connection_destroy(skypewebcon);
		purple_connection_ssl_error(pc, errortype);
	}
}

SkypeWebConnection *
skypeweb_post_or_get(SkypeWebAccount *sa, SkypeWebMethod method,
		const gchar *host, const gchar *url, const gchar *postdata,
		SkypeWebProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive)
{
	GString *request;
	gchar *cookies;
	SkypeWebConnection *skypewebcon;
	gchar *real_url;
	gboolean is_proxy = FALSE;
	//const gchar *user_agent;
	const gchar* const *languages;
	gchar *language_names;
	PurpleProxyInfo *proxy_info = NULL;
	gchar *proxy_auth;
	gchar *proxy_auth_base64;

	/* TODO: Fix keepalive and use it as much as possible */
	keepalive = FALSE;

	if (host == NULL)
		host = "api.skype.com";

	if (sa && sa->account)
	{
		if (purple_account_get_bool(sa->account, "use-https", TRUE))
			method |= SKYPEWEB_METHOD_SSL;
	}

	if (sa && sa->account && !(method & SKYPEWEB_METHOD_SSL))
	{
		proxy_info = purple_proxy_get_setup(sa->account);
		if (purple_proxy_info_get_proxy_type(proxy_info) == PURPLE_PROXY_USE_GLOBAL)
			proxy_info = purple_global_proxy_get_info();
		if (purple_proxy_info_get_proxy_type(proxy_info) == PURPLE_PROXY_HTTP)
		{
			is_proxy = TRUE;
		}	
	}
	if (is_proxy == TRUE)
	{
		real_url = g_strdup_printf("http://%s%s", host, url);
	} else {
		real_url = g_strdup(url);
	}

	cookies = skypeweb_cookies_to_string(sa);
	//user_agent = purple_account_get_string(sa->account, "user-agent", "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.71 Safari/537.36");
	
	if ((method & (SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_PUT)) && !postdata)
		postdata = "";

	/* Build the request */
	request = g_string_new(NULL);
	g_string_append_printf(request, "%s %s HTTP/1.0\r\n",
			((method & SKYPEWEB_METHOD_POST) ? "POST" : ((method & SKYPEWEB_METHOD_PUT) ? "PUT" : ((method & SKYPEWEB_METHOD_DELETE) ? "DELETE" : "GET"))),
			real_url);
	if (is_proxy == FALSE)
		g_string_append_printf(request, "Host: %s\r\n", host);
	g_string_append_printf(request, "Connection: %s\r\n",
			(keepalive ? "Keep-Alive" : "close"));
	//g_string_append_printf(request, "User-Agent: %s\r\n", user_agent);
	if (method & (SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_PUT)) {		
		if (postdata && (postdata[0] == '[' || postdata[0] == '{')) {
			g_string_append(request, "Content-Type: application/json\r\n"); // hax
		} else {
			g_string_append_printf(request, "Content-Type: application/x-www-form-urlencoded\r\n");
		}
		g_string_append_printf(request, "Content-length: %" G_GSIZE_FORMAT "\r\n", strlen(postdata));
	}
	
	if (g_str_equal(host, SKYPEWEB_CONTACTS_HOST) || g_str_equal(host, SKYPEWEB_VIDEOMAIL_HOST) || g_str_equal(host, SKYPEWEB_NEW_CONTACTS_HOST)) {
		g_string_append_printf(request, "X-Skypetoken: %s\r\n", sa->skype_token);
		g_string_append(request, "X-Stratus-Caller: " SKYPEWEB_CLIENTINFO_NAME "\r\n");
		g_string_append(request, "X-Stratus-Request: abcd1234\r\n");
		g_string_append(request, "Origin: https://web.skype.com\r\n");
		g_string_append(request, "Referer: https://web.skype.com/main\r\n");
		g_string_append(request, "Accept: application/json; ver=1.0;\r\n");
	} else if (g_str_equal(host, SKYPEWEB_GRAPH_HOST)) {
		g_string_append_printf(request, "X-Skypetoken: %s\r\n", sa->skype_token);
		g_string_append(request, "Accept: application/json\r\n");
	} else if (g_str_equal(host, sa->messages_host)) {
		g_string_append_printf(request, "RegistrationToken: %s\r\n", sa->registration_token);
		g_string_append(request, "Referer: https://web.skype.com/main\r\n");
		g_string_append(request, "Accept: application/json; ver=1.0;\r\n");
		g_string_append(request, "ClientInfo: os=Windows; osVer=8.1; proc=Win32; lcid=en-us; deviceType=1; country=n/a; clientName=" SKYPEWEB_CLIENTINFO_NAME "; clientVer=" SKYPEWEB_CLIENTINFO_VERSION "\r\n");
	} else {
		g_string_append_printf(request, "Accept: */*\r\n");
		g_string_append_printf(request, "Cookie: %s\r\n", cookies);
	}
	
	g_string_append_printf(request, "Accept-Encoding: gzip\r\n");
	if (is_proxy == TRUE)
	{
		if (purple_proxy_info_get_username(proxy_info) &&
			purple_proxy_info_get_password(proxy_info))
		{
			proxy_auth = g_strdup_printf("%s:%s", purple_proxy_info_get_username(proxy_info), purple_proxy_info_get_password(proxy_info));
			proxy_auth_base64 = purple_base64_encode((guchar *)proxy_auth, strlen(proxy_auth));
			g_string_append_printf(request, "Proxy-Authorization: Basic %s\r\n", proxy_auth_base64);
			g_free(proxy_auth_base64);
			g_free(proxy_auth);
		}
	}

	/* Tell the server what language we accept, so that we get error messages in our language (rather than our IP's) */
	languages = g_get_language_names();
	language_names = g_strjoinv(", ", (gchar **)languages);
	purple_util_chrreplace(language_names, '_', '-');
	g_string_append_printf(request, "Accept-Language: %s\r\n", language_names);
	g_free(language_names);

	purple_debug_info("skypeweb", "getting url %s\n", url);

	g_string_append_printf(request, "\r\n");
	if (method & (SKYPEWEB_METHOD_POST | SKYPEWEB_METHOD_PUT))
		g_string_append_printf(request, "%s", postdata);

	/* If it needs to go over a SSL connection, we probably shouldn't print
	 * it in the debug log.  Without this condition a user's password is
	 * printed in the debug log */
	if (method == SKYPEWEB_METHOD_POST || method == SKYPEWEB_METHOD_PUT)
		purple_debug_info("skypeweb", "sending request data:\n%s\n", postdata);
	
	purple_debug_misc("skypeweb", "sending headers:\n%s\n", request->str);

	g_free(cookies);

	skypewebcon = g_new0(SkypeWebConnection, 1);
	skypewebcon->sa = sa;
	skypewebcon->url = real_url;
	skypewebcon->method = method;
	skypewebcon->hostname = g_strdup(host);
	skypewebcon->request = request;
	skypewebcon->callback = callback_func;
	skypewebcon->user_data = user_data;
	skypewebcon->fd = -1;
	skypewebcon->connection_keepalive = keepalive;
	skypewebcon->request_time = time(NULL);
	
	g_queue_push_head(sa->waiting_conns, skypewebcon);
	skypeweb_next_connection(sa);
	
	return skypewebcon;
}

static void skypeweb_next_connection(SkypeWebAccount *sa)
{
	SkypeWebConnection *skypewebcon;
	
	g_return_if_fail(sa != NULL);	
	
	if (!g_queue_is_empty(sa->waiting_conns))
	{
		if(g_slist_length(sa->conns) < SKYPEWEB_MAX_CONNECTIONS)
		{
			skypewebcon = g_queue_pop_tail(sa->waiting_conns);
			skypeweb_attempt_connection(skypewebcon);
		}
	}
}


static gboolean
skypeweb_connection_timedout(gpointer userdata)
{
	SkypeWebConnection *skypewebcon = userdata;
	SkypeWebAccount *sa = skypewebcon->sa;
	
	/* Try resend the request */
	skypewebcon->retry_count++;
	if (skypewebcon->retry_count < 3) {
		skypeweb_connection_close(skypewebcon);
		skypewebcon->request_time = time(NULL);
		
		g_queue_push_head(sa->waiting_conns, skypewebcon);
		skypeweb_next_connection(sa);
	} else {
		skypeweb_fatal_connection_cb(skypewebcon);
	}
	
	return FALSE;
}

static void skypeweb_attempt_connection(SkypeWebConnection *skypewebcon)
{
	gboolean is_proxy = FALSE;
	SkypeWebAccount *sa = skypewebcon->sa;
	PurpleProxyInfo *proxy_info = NULL;

	if (sa && sa->account && !(skypewebcon->method & SKYPEWEB_METHOD_SSL))
	{
		proxy_info = purple_proxy_get_setup(sa->account);
		if (purple_proxy_info_get_proxy_type(proxy_info) == PURPLE_PROXY_USE_GLOBAL)
			proxy_info = purple_global_proxy_get_info();
		if (purple_proxy_info_get_proxy_type(proxy_info) == PURPLE_PROXY_HTTP)
		{
			is_proxy = TRUE;
		}	
	}

#if 0
	/* Connection to attempt retries.  This code doesn't work perfectly, but
	 * remains here for future reference if needed */
	if (time(NULL) - skypewebcon->request_time > 5) {
		/* We've continuously tried to remake this connection for a 
		 * bit now.  It isn't happening, sadly.  Time to die. */
		purple_debug_error("skypeweb", "could not connect after retries\n");
		skypeweb_fatal_connection_cb(skypewebcon);
		return;
	}

	purple_debug_info("skypeweb", "making connection attempt\n");

	/* TODO: If we're retrying the connection, consider clearing the cached
	 * DNS value.  This will require some juggling with the hostname param */
	/* TODO/FIXME: This retries almost instantenously, which in some cases
	 * runs at blinding speed.  Slow it down. */
	/* TODO/FIXME: this doesn't retry properly on non-ssl connections */
#endif
	
	sa->conns = g_slist_prepend(sa->conns, skypewebcon);

	/*
	 * Do a separate DNS lookup for the given host name and cache it
	 * for next time.
	 *
	 * TODO: It would be better if we did this before we call
	 *       purple_proxy_connect(), so we could re-use the result.
	 *       Or even better: Use persistent HTTP connections for servers
	 *       that we access continually.
	 *
	 * TODO: This cache of the hostname<-->IP address does not respect
	 *       the TTL returned by the DNS server.  We should expire things
	 *       from the cache after some amount of time.
	 */
	if (!is_proxy && !(skypewebcon->method & SKYPEWEB_METHOD_SSL) && !g_hostname_is_ip_address(skypewebcon->hostname))
	{
		/* Don't do this for proxy connections, since proxies do the DNS lookup */
		gchar *host_ip;

		host_ip = g_hash_table_lookup(sa->hostname_ip_cache, skypewebcon->hostname);
		if (host_ip != NULL) {
			g_free(skypewebcon->hostname);
			skypewebcon->hostname = g_strdup(host_ip);
		} else if (sa->account && !purple_account_is_disconnecting(sa->account)) {
			GSList *host_lookup_list = NULL;
			PurpleDnsQueryData *query;

			host_lookup_list = g_slist_prepend(
					host_lookup_list, g_strdup(skypewebcon->hostname));
			host_lookup_list = g_slist_prepend(
					host_lookup_list, sa);

			query = purple_dnsquery_a(
#if PURPLE_VERSION_CHECK(3, 0, 0)
					skypewebcon->sa->account,
#endif
					skypewebcon->hostname, 80,
					skypeweb_host_lookup_cb, host_lookup_list);
			sa->dns_queries = g_slist_prepend(sa->dns_queries, query);
			host_lookup_list = g_slist_append(host_lookup_list, query);
		}
	}

	if (skypewebcon->method & SKYPEWEB_METHOD_SSL) {
		skypewebcon->ssl_conn = purple_ssl_connect(sa->account, skypewebcon->hostname,
				443, skypeweb_post_or_get_ssl_connect_cb,
				skypeweb_ssl_connection_error, skypewebcon);
	} else {
		skypewebcon->connect_data = purple_proxy_connect(NULL, sa->account,
				skypewebcon->hostname, 80, skypeweb_post_or_get_connect_cb, skypewebcon);
	}
	
	skypewebcon->timeout_watcher = purple_timeout_add_seconds(120, skypeweb_connection_timedout, skypewebcon);

	return;
}

