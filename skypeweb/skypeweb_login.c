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

#include "skypeweb_login.h"
#include "skypeweb_util.h"


static void
skypeweb_login_did_auth(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	gchar *refresh_token = NULL;
	SkypeWebAccount *sa = user_data;
	
	sa->url_datas = g_slist_remove(sa->url_datas, url_data);

	if (url_text == NULL) {
		url_text = url_data->webdata;
		len = url_data->data_len;
	}
	
	if (url_text != NULL)
		refresh_token = skypeweb_string_get_chunk(url_text, len, "=\"skypetoken\" value=\"", "\"");
	
	if (refresh_token == NULL) {
		if (g_strstr_len(url_text, len, "recaptcha_response_field")) {
			purple_connection_error(sa->pc,
									PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
									_("Captcha required.\nTry logging into web.skype.com and try again."));
			return;
		} else {
			purple_debug_info("skypeweb", "login response was %s\r\n", url_text);
			purple_connection_error(sa->pc,
									PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
									_("Failed getting Skype Token"));
			return;
		}
	}
	
	sa->skype_token = refresh_token;
	
	skypeweb_update_cookies(sa, url_text);
	
	skypeweb_do_all_the_things(sa);
}

static void
skypeweb_login_got_pie(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	SkypeWebAccount *sa = user_data;
	PurpleAccount *account = sa->account;
	gchar *pie;
	gchar *etm;
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST;// "/login?client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com";
	GString *postdata;
	gchar *request;
	struct timeval tv;
	struct timezone tz;
	gint tzhours, tzminutes;
	
	sa->url_datas = g_slist_remove(sa->url_datas, url_data);

	if (error_message && *error_message) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_message);
		return;
	}
	
	gettimeofday(&tv, &tz);
	(void) tv;
	tzminutes = tz.tz_minuteswest;
	if (tzminutes < 0) tzminutes = -tzminutes;
	tzhours = tzminutes / 60;
	tzminutes -= tzhours * 60;
	
	pie = skypeweb_string_get_chunk(url_text, len, "=\"pie\" value=\"", "\"");
	if (!pie) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting PIE value"));
		return;
	}
	
	etm = skypeweb_string_get_chunk(url_text, len, "=\"etm\" value=\"", "\"");
	if (!etm) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting ETM value"));
		return;
	}
	
	
	postdata = g_string_new("");
	g_string_append_printf(postdata, "username=%s&", purple_url_encode(purple_account_get_username(account)));
	g_string_append_printf(postdata, "password=%s&", purple_url_encode(purple_account_get_password(account)));
	g_string_append_printf(postdata, "timezone_field=%c|%d|%d&", (tz.tz_minuteswest < 0 ? '+' : '-'), tzhours, tzminutes);
	g_string_append_printf(postdata, "pie=%s&", purple_url_encode(pie));
	g_string_append_printf(postdata, "etm=%s&", purple_url_encode(etm));
	g_string_append_printf(postdata, "js_time=%" G_GINT64_FORMAT "&", skypeweb_get_js_time());
	g_string_append(postdata, "client_id=578134&");
	g_string_append(postdata, "redirect_uri=https://web.skype.com/");
	
	request = g_strdup_printf("POST /login?client_id=578134&redirect_uri=https%%3A%%2F%%2Fweb.skype.com HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: */*\r\n"
			"BehaviorOverride: redirectAs404\r\n"
			"Host: " SKYPEWEB_LOGIN_HOST "\r\n"
			"Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n"
			"Content-Length: %" G_GSIZE_FORMAT "\r\n\r\n%s",
			strlen(postdata->str), postdata->str);
	
	skypeweb_fetch_url_request(sa, login_url, TRUE, NULL, FALSE, request, TRUE, 524288, skypeweb_login_did_auth, sa);

	g_string_free(postdata, TRUE);
	g_free(request);
	
	g_free(pie);
	g_free(etm);
	
	purple_connection_update_progress(sa->pc, _("Authenticating"), 2, 4);
}

void
skypeweb_begin_web_login(SkypeWebAccount *sa)
{
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login?method=skype&client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com";
	
	skypeweb_fetch_url_request(sa, login_url, TRUE, NULL, FALSE, NULL, FALSE, 524288, skypeweb_login_got_pie, sa);
	
	purple_connection_set_state(sa->pc, PURPLE_CONNECTION_CONNECTING);
	purple_connection_update_progress(sa->pc, _("Connecting"), 1, 4);
}

static void
skypeweb_login_got_t(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	SkypeWebAccount *sa = user_data;
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST;// "/login/oauth?client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com";
	gchar *request;
	GString *postdata;
	gchar *magic_t_value; // T is for tasty

	sa->url_datas = g_slist_remove(sa->url_datas, url_data);
	
	// <input type="hidden" name="t" id="t" value="...">
	magic_t_value = skypeweb_string_get_chunk(url_text, len, "=\"t\" value=\"", "\"");
	if (!magic_t_value) {
		//No Magic T????  Maybe it be the mighty 2fa-beast
		
		if (FALSE)
		/*if (g_strnstr(url_text, len, "Set-Cookie: LOpt=0;"))*/ {
			//XX - Would this be better retrieved with JSON decoding the "var ServerData = {...}" code?
			//     <script type="text/javascript">var ServerData = {...};</script>
			gchar *session_state = skypeweb_string_get_chunk(url_text, len, ":'https://login.live.com/GetSessionState.srf?", "',");
			if (session_state) {
				//These two appear to have different object keys each request :(
				gchar *PPFT = skypeweb_string_get_chunk(url_text, len, ",sFT:'", "',");
				gchar *SLK = skypeweb_string_get_chunk(url_text, len, ",aB:'", "',");
				gchar *ppauth_cookie = skypeweb_string_get_chunk(url_text, len, "Set-Cookie: PPAuth=", ";");
				gchar *mspok_cookie = skypeweb_string_get_chunk(url_text, len, "Set-Cookie: MSPOK=", "; domain=");
				
				//Poll https://login.live.com/GetSessionState.srv?{session_state} to retrieve GIF(!!) of 2fa status
				//1x1 size GIF means pending, 2x2 rejected, 1x2 approved
				//Then re-request the MagicT, if approved with a slightly different GET parameters
				//purpose=eOTT_OneTimePassword&PPFT={ppft}&login={email}&SLK={slk}
				return;
			}
		}
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting Magic T value"));
		return;
	}
	
	// postdata: t=...&oauthPartner=999&client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com
	postdata = g_string_new("");
	g_string_append_printf(postdata, "t=%s&", purple_url_encode(magic_t_value));
	g_string_append(postdata, "site_name=lw.skype.com&");
	g_string_append(postdata, "oauthPartner=999&");
	g_string_append(postdata, "client_id=578134&");
	g_string_append(postdata, "redirect_uri=https%3A%2F%2Fweb.skype.com");
	
	// post the t to https://login.skype.com/login/oauth?client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com
	request = g_strdup_printf("POST /login/microsoft?client_id=578134&redirect_uri=https%%3A%%2F%%2Fweb.skype.com HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: */*\r\n"
			"BehaviorOverride: redirectAs404\r\n"
			"Host: " SKYPEWEB_LOGIN_HOST "\r\n"
			"Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n"
			"Content-Length: %" G_GSIZE_FORMAT "\r\n\r\n%s",
			strlen(postdata->str), postdata->str);
	
	skypeweb_fetch_url_request(sa, login_url, TRUE, NULL, FALSE, request, TRUE, 524288, skypeweb_login_did_auth, sa);
	
	g_string_free(postdata, TRUE);
	g_free(request);
	g_free(magic_t_value);
	
	purple_connection_update_progress(sa->pc, _("Verifying"), 3, 4);
}

static void
skypeweb_login_got_ppft(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	SkypeWebAccount *sa = user_data;
	const gchar *live_login_url = "https://login.live.com";// "/ppsecure/post.srf?wa=wsignin1.0&wreply=https%3A%2F%2Fsecure.skype.com%2Flogin%2Foauth%2Fproxy%3Fclient_id%3D578134%26redirect_uri%3Dhttps%253A%252F%252Fweb.skype.com";
	gchar *msprequ_cookie;
	gchar *mspok_cookie;
	gchar *cktst_cookie;
	gchar *ppft;
	GString *postdata;
	gchar *request;

	sa->url_datas = g_slist_remove(sa->url_datas, url_data);
	
	// grab PPFT and cookies (MSPRequ, MSPOK)
	msprequ_cookie = skypeweb_string_get_chunk(url_text, len, "Set-Cookie: MSPRequ=", ";");
	if (!msprequ_cookie) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting MSPRequ cookie"));
		return;
	}
	mspok_cookie = skypeweb_string_get_chunk(url_text, len, "Set-Cookie: MSPOK=", ";");
	if (!mspok_cookie) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting MSPOK cookie"));
		return;
	}
	// <input type="hidden" name="PPFT" id="i0327" value="..."/>
	ppft = skypeweb_string_get_chunk(url_text, len, "name=\"PPFT\" id=\"i0327\" value=\"", "\"");
	if (!ppft) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting PPFT value"));
		return;
	}
	// CkTst=G + timestamp   e.g. G1422309314913
	cktst_cookie = g_strdup_printf("G%" G_GINT64_FORMAT, skypeweb_get_js_time());
	
	// postdata: login={username}&passwd={password}&PPFT={ppft value}
	postdata = g_string_new("");
	g_string_append_printf(postdata, "login=%s&", purple_url_encode(purple_account_get_username(sa->account)));
	g_string_append_printf(postdata, "passwd=%s&", purple_url_encode(purple_account_get_password(sa->account)));
	g_string_append_printf(postdata, "PPFT=%s&", purple_url_encode(ppft));
	
	// POST to https://login.live.com/ppsecure/post.srf?wa=wsignin1.0&wreply=https%3A%2F%2Fsecure.skype.com%2Flogin%2Foauth%2Fproxy%3Fclient_id%3D578134%26redirect_uri%3Dhttps%253A%252F%252Fweb.skype.com
	
	request = g_strdup_printf("POST /ppsecure/post.srf?wa=wsignin1.0&wp=MBI_SSL&wreply=https%%3A%%2F%%2Fsecure.skype.com%%2Flogin%%2Foauth%%2Fproxy%%3Fclient_id%%3D578134%%26redirect_uri%%3Dhttps%%253A%%252F%%252Fweb.skype.com HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: */*\r\n"
			"Host: login.live.com\r\n"
			"Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n"
			"Cookie: MSPRequ=%s;MSPOK=%s;CkTst=%s;\r\n"
			"Content-Length: %" G_GSIZE_FORMAT "\r\n\r\n%s",
			msprequ_cookie, mspok_cookie, cktst_cookie, strlen(postdata->str), postdata->str);
	
	skypeweb_fetch_url_request(sa, live_login_url, TRUE, NULL, FALSE, request, FALSE, 524288, skypeweb_login_got_t, sa);
	
	g_string_free(postdata, TRUE);
	g_free(request);
	
	g_free(msprequ_cookie);
	g_free(mspok_cookie);
	g_free(cktst_cookie);
	g_free(ppft);
	
	purple_connection_update_progress(sa->pc, _("Authenticating"), 2, 4);
}

void
skypeweb_begin_oauth_login(SkypeWebAccount *sa)
{
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login/oauth/microsoft?client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com";
	
	skypeweb_fetch_url_request(sa, login_url, TRUE, NULL, FALSE, NULL, TRUE, 524288, skypeweb_login_got_ppft, sa);
	
	purple_connection_set_state(sa->pc, PURPLE_CONNECTION_CONNECTING);
	purple_connection_update_progress(sa->pc, _("Connecting"), 1, 4);
}

void
skypeweb_logout(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_LOGIN_HOST, "/logout", NULL, NULL, NULL, TRUE);
}
