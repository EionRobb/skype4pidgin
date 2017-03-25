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
 
#include "skypeweb_util.h"

#if !PURPLE_VERSION_CHECK(3, 0, 0)
#	include "cipher.h"
#else
#	include "ciphers/sha256hash.h"
#endif

gchar *
skypeweb_string_get_chunk(const gchar *haystack, gsize len, const gchar *start, const gchar *end)
{
	const gchar *chunk_start, *chunk_end;
	g_return_val_if_fail(haystack && start && end, NULL);
	
	if (len > 0) {
		chunk_start = g_strstr_len(haystack, len, start);
	} else {
		chunk_start = strstr(haystack, start);
	}
	g_return_val_if_fail(chunk_start, NULL);
	chunk_start += strlen(start);
	
	if (len > 0) {
		chunk_end = g_strstr_len(chunk_start, len - (chunk_start - haystack), end);
	} else {
		chunk_end = strstr(chunk_start, end);
	}
	g_return_val_if_fail(chunk_end, NULL);
	
	return g_strndup(chunk_start, chunk_end - chunk_start);
}

gchar *
skypeweb_jsonobj_to_string(JsonObject *jsonobj)
{
	JsonGenerator *generator;
	JsonNode *root;
	gchar *string;
	
	root = json_node_new(JSON_NODE_OBJECT);
	json_node_set_object(root, jsonobj);
	
	generator = json_generator_new();
	json_generator_set_root(generator, root);
	
	string = json_generator_to_data(generator, NULL);
	
	g_object_unref(generator);
	json_node_free(root);
	
	return string;
}

/** turn https://bay-client-s.gateway.messenger.live.com/v1/users/ME/contacts/8:eionrobb 
      or https://bay-client-s.gateway.messenger.live.com/v1/users/8:eionrobb/presenceDocs/messagingService 
	into eionrobb
*/
const gchar *
skypeweb_contact_url_to_name(const gchar *url)
{
	static gchar *tempname = NULL;
	const gchar *start, *end;
	
	start = g_strrstr(url, "/8:");
	if (!start) start = g_strrstr(url, "/1:");
	if (!start) start = g_strrstr(url, "/4:");
	if (start) start = start + 2;
	if (!start) start = g_strrstr(url, "/2:");
	if (start) start = start + 1;
	if (!start) return NULL;
	
	if ((end = strchr(start, '/'))) {
		g_free(tempname);
		tempname = g_strndup(start, end - start);
		return tempname;
	}
	
	g_free(tempname);
	tempname = g_strdup(start);
	return tempname;
}

/** turn https://bay-client-s.gateway.messenger.live.com/v1/users/ME/conversations/19:blah@thread.skype
	into 19:blah@thread.skype
*/
const gchar *
skypeweb_thread_url_to_name(const gchar *url)
{
	static gchar *tempname = NULL;
	const gchar *start, *end;
	
	start = g_strrstr(url, "/19:");
	if (!start) return NULL;
	start = start + 1;
	
	if ((end = strchr(start, '/'))) {
		g_free(tempname);
		tempname = g_strndup(start, end - start);
		return tempname;
	}
	
	return start;
}

/** Blatantly stolen from MSN prpl, with super-secret SHA256 change! */
#define BUFSIZE	256
char *
skypeweb_hmac_sha256(char *input)
{
	PurpleHash *hash;
	const guchar productKey[] = SKYPEWEB_LOCKANDKEY_SECRET;
	const guchar productID[]  = SKYPEWEB_LOCKANDKEY_APPID;
	const char hexChars[]     = "0123456789abcdef";
	char buf[BUFSIZE];
	unsigned char sha256Hash[32];
	unsigned char *newHash;
	unsigned int *sha256Parts;
	unsigned int *chlStringParts;
	unsigned int newHashParts[5];
	gchar *output;

	long long nHigh = 0, nLow = 0;

	int len;
	int i;
	
	hash = purple_sha256_hash_new();
	purple_hash_append(hash, (guchar *)input, strlen(input));
	purple_hash_append(hash, productKey, sizeof(productKey) - 1);
	purple_hash_digest(hash, (guchar *)sha256Hash, sizeof(sha256Hash));
	purple_hash_destroy(hash);
	
	/* Split it into four integers */
	sha256Parts = (unsigned int *)sha256Hash;
	for (i = 0; i < 4; i++) {
		/* adjust endianess */
		sha256Parts[i] = GUINT_TO_LE(sha256Parts[i]);

		/* & each integer with 0x7FFFFFFF          */
		/* and save one unmodified array for later */
		newHashParts[i] = sha256Parts[i];
		sha256Parts[i] &= 0x7FFFFFFF;
	}

	/* make a new string and pad with '0' to length that's a multiple of 8 */
	snprintf(buf, BUFSIZE - 5, "%s%s", input, productID);
	len = strlen(buf);
	if ((len % 8) != 0) {
		int fix = 8 - (len % 8);
		memset(&buf[len], '0', fix);
		buf[len + fix] = '\0';
		len += fix;
	}

	/* split into integers */
	chlStringParts = (unsigned int *)buf;

	/* this is magic */
	for (i = 0; i < (len / 4); i += 2) {
		long long temp;

		chlStringParts[i] = GUINT_TO_LE(chlStringParts[i]);
		chlStringParts[i + 1] = GUINT_TO_LE(chlStringParts[i + 1]);

		temp = (0x0E79A9C1 * (long long)chlStringParts[i]) % 0x7FFFFFFF;
		temp = (sha256Parts[0] * (temp + nLow) + sha256Parts[1]) % 0x7FFFFFFF;
		nHigh += temp;

		temp = ((long long)chlStringParts[i + 1] + temp) % 0x7FFFFFFF;
		nLow = (sha256Parts[2] * temp + sha256Parts[3]) % 0x7FFFFFFF;
		nHigh += nLow;
	}
	nLow = (nLow + sha256Parts[1]) % 0x7FFFFFFF;
	nHigh = (nHigh + sha256Parts[3]) % 0x7FFFFFFF;

	newHashParts[0] ^= nLow;
	newHashParts[1] ^= nHigh;
	newHashParts[2] ^= nLow;
	newHashParts[3] ^= nHigh;

	/* adjust endianness */
	for(i = 0; i < 4; i++)
		newHashParts[i] = GUINT_TO_LE(newHashParts[i]);
	
	/* make a string of the parts */
	newHash = (unsigned char *)newHashParts;
	
	/* convert to hexadecimal */
	output = g_new0(gchar, 33);
	for (i = 0; i < 16; i++)
	{
		output[i * 2] = hexChars[(newHash[i] >> 4) & 0xF];
		output[(i * 2) + 1] = hexChars[newHash[i] & 0xF];
	}
	output[32] = '\0';
	
	return output;
}

gint64
skypeweb_get_js_time()
{
#if GLIB_CHECK_VERSION(2, 28, 0)
	return (g_get_real_time() / 1000);
#else
	GTimeVal val;
	
	g_get_current_time (&val);
	
	return (((gint64) val.tv_sec) * 1000) + (val.tv_usec / 1000);
#endif
}

/* copied from oscar.c to be libpurple 2.1 compatible */
PurpleAccount *
find_acct(const char *prpl, const char *acct_id)
{
	PurpleAccount *acct = NULL;
	
	/* If we have a specific acct, use it */
	if (acct_id && *acct_id) {
		acct = purple_accounts_find(acct_id, prpl);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(prpl, purple_account_get_protocol_id(l->data))
				&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}
	
	return acct;
}


/* Hack needed to stop redirect */
struct _PurpleUtilFetchUrlData
{
	PurpleUtilFetchUrlCallback callback;
	void *user_data;

	struct
	{
		char *user;
		char *passwd;
		char *address;
		int port;
		char *page;

	} website;

	char *url;
	int num_times_redirected;
	gboolean full;
	char *user_agent;
	gboolean http11;
	char *request;
	gsize request_written;
	gboolean include_headers;

	gboolean is_ssl;
	PurpleSslConnection *ssl_connection;
	PurpleProxyConnectData *connect_data;
	int fd;
	guint inpa;

	gboolean got_headers;
	gboolean has_explicit_data_len;
	char *webdata;
	gsize len;
	unsigned long data_len;
	gssize max_len;
	gboolean chunked;
	PurpleAccount *account;
};

/* Hack needed to stop redirect */
struct _PurpleUtilFetchUrlDataTwoEleven
{
	PurpleUtilFetchUrlCallback callback;
	void *user_data;

	struct
	{
		char *user;
		char *passwd;
		char *address;
		int port;
		char *page;

	} website;

	char *url;
	int num_times_redirected;
	gboolean full;
	char *user_agent;
	gboolean http11;
	char *request;
	gsize request_len;
	gsize request_written;
	gboolean include_headers;

	gboolean is_ssl;
	PurpleSslConnection *ssl_connection;
	PurpleProxyConnectData *connect_data;
	int fd;
	guint inpa;

	gboolean got_headers;
	gboolean has_explicit_data_len;
	char *webdata;
	gsize len;
	unsigned long data_len;
	gssize max_len;
	gboolean chunked;
	PurpleAccount *account;
};

static void
skypeweb_fetch_url_request_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message) {
	
	PurpleUtilFetchUrlCallback callback;
	
	if (url_text == NULL) {
		if (purple_major_version == 2 && purple_minor_version >= 11) {
			struct _PurpleUtilFetchUrlDataTwoEleven *two_eleven_url_data = (struct _PurpleUtilFetchUrlDataTwoEleven *) url_data;
			
			url_text = two_eleven_url_data->webdata;
			len = two_eleven_url_data->data_len;
		} else {
			url_text = url_data->webdata;
			len = url_data->data_len;
		}
	}
	
	callback = g_dataset_get_data(url_data, "real_callback");
	callback(url_data, user_data, url_text, len, error_message);
	
	g_dataset_destroy(url_data);
}

/* Wrapper of purple_util_fetch_url_request_len_with_account()
 * that takes a SkypeWebAccount instead of a PurpleAccount,
 * and keeps track of requests in sa->url_datas to cancel them on logout. */

PurpleUtilFetchUrlData *
skypeweb_fetch_url_request(SkypeWebAccount *sa,
		const char *url, gboolean full, const char *user_agent, gboolean http11,
		const char *request, gboolean include_headers, gssize max_len,
		PurpleUtilFetchUrlCallback callback, void *user_data)
{
	PurpleUtilFetchUrlData *url_data;

	url_data = purple_util_fetch_url_request(sa->account, url, full, user_agent, http11, request, include_headers, max_len, skypeweb_fetch_url_request_cb, user_data);
	g_dataset_set_data(url_data, "real_callback", callback);

	if (url_data != NULL)
		sa->url_datas = g_slist_prepend(sa->url_datas, url_data);

	return url_data;
}

void
skypeweb_url_prevent_follow_redirects(PurpleUtilFetchUrlData *requestdata)
{
	if (requestdata != NULL) {
		requestdata->num_times_redirected = 10;
	}
}

const gchar *
skypeweb_user_url_prefix(const gchar *who)
{
	if(SKYPEWEB_BUDDY_IS_S4B(who) || SKYPEWEB_BUDDY_IS_BOT(who)) {
		return ""; // already has a prefix
	} else if (SKYPEWEB_BUDDY_IS_MSN(who)) {
		return "1:";
	} else if(SKYPEWEB_BUDDY_IS_PHONE(who)) {
		return "4:";
	} else {
		return "8:";
	}
}

const gchar *
skypeweb_strip_user_prefix(const gchar *who)
{
	if (who[1] == ':') {
		if (who[0] != '2') {
			return who + 2;
		}
	}
	
	return who;
}