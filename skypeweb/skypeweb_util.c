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

#include <cipher.h>

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
	if (!start) return NULL;
	start = start + 3;
	
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
	PurpleCipher *cipher;
	PurpleCipherContext *context;
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
	
	/* Create the SHA256 hash by using Purple SHA256 algorithm */
	cipher = purple_ciphers_find_cipher("sha256");
	context = purple_cipher_context_new(cipher, NULL);

	purple_cipher_context_append(context, (guchar *)input, strlen(input));
	purple_cipher_context_append(context, productKey, sizeof(productKey) - 1);
	purple_cipher_context_digest(context, sizeof(sha256Hash), sha256Hash, NULL);
	purple_cipher_context_destroy(context);
	
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
	GTimeVal val;
	
	g_get_current_time (&val);
	
	return (((gint64) val.tv_sec) * 1000) + (val.tv_usec / 1000);
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
