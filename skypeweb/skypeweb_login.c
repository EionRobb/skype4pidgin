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
skypeweb_login_did_auth(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	gchar *refresh_token = NULL;
	SkypeWebAccount *sa = user_data;
	const gchar *data;
	gsize len;
	
	data = purple_http_response_get_data(response, &len);
	
	if (data != NULL) {
		refresh_token = skypeweb_string_get_chunk(data, len, "=\"skypetoken\" value=\"", "\"");
	} else {
		purple_connection_error(sa->pc,
								PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
								_("Failed getting Skype Token"));
	}
	
	if (refresh_token == NULL) {
		purple_account_set_string(sa->account, "refresh-token", NULL);
		if (g_strstr_len(data, len, "recaptcha_response_field")) {
			purple_connection_error(sa->pc,
									PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
									_("Captcha required.\nTry logging into web.skype.com and try again."));
			return;
		} else {
			purple_debug_info("skypeweb", "login response was %s\r\n", data);
			purple_connection_error(sa->pc,
									PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
									_("Failed getting Skype Token"));
			return;
		}
	}
	
	sa->skype_token = refresh_token;
	
	if (purple_account_get_remember_password(sa->account)) {
		purple_account_set_string(sa->account, "refresh-token", purple_http_cookie_jar_get(sa->cookie_jar, "refresh-token"));
	}
	
	skypeweb_do_all_the_things(sa);
}

static void
skypeweb_login_got_pie(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	SkypeWebAccount *sa = user_data;
	PurpleAccount *account = sa->account;
	gchar *pie;
	gchar *etm;
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login?client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com";
	GString *postdata;
	struct timeval tv;
	struct timezone tz;
	gint tzhours, tzminutes;
	int tmplen;
	PurpleHttpRequest *request;
	const gchar *data;
	gsize len;
	
	if (!purple_http_response_is_successful(response)) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, purple_http_response_get_error(response));
		return;
	}
	
	data = purple_http_response_get_data(response, &len);
	
	gettimeofday(&tv, &tz);
	(void) tv;
	tzminutes = tz.tz_minuteswest;
	if (tzminutes < 0) tzminutes = -tzminutes;
	tzhours = tzminutes / 60;
	tzminutes -= tzhours * 60;
	
	pie = skypeweb_string_get_chunk(data, len, "=\"pie\" value=\"", "\"");
	if (!pie) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting PIE value"));
		return;
	}
	
	etm = skypeweb_string_get_chunk(data, len, "=\"etm\" value=\"", "\"");
	if (!etm) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting ETM value"));
		return;
	}
	
	
	postdata = g_string_new("");
	g_string_append_printf(postdata, "username=%s&", purple_url_encode(purple_account_get_username(account)));
	g_string_append_printf(postdata, "password=%s&", purple_url_encode(purple_connection_get_password(sa->pc)));
	g_string_append_printf(postdata, "timezone_field=%c|%d|%d&", (tz.tz_minuteswest < 0 ? '+' : '-'), tzhours, tzminutes);
	g_string_append_printf(postdata, "pie=%s&", purple_url_encode(pie));
	g_string_append_printf(postdata, "etm=%s&", purple_url_encode(etm));
	g_string_append_printf(postdata, "js_time=%" G_GINT64_FORMAT "&", skypeweb_get_js_time());
	g_string_append(postdata, "client_id=578134&");
	g_string_append(postdata, "redirect_uri=https://web.skype.com/");

	tmplen = postdata->len;
	if (postdata->len > INT_MAX) tmplen = INT_MAX;
	
	request = purple_http_request_new(login_url);
	purple_http_request_set_method(request, "POST");
	purple_http_request_set_cookie_jar(request, sa->cookie_jar);
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
	purple_http_request_header_set(request, "Accept", "*/*");
	purple_http_request_header_set(request, "BehaviorOverride", "redirectAs404");
	purple_http_request_set_contents(request, postdata->str, tmplen);
	purple_http_request(sa->pc, request, skypeweb_login_did_auth, sa);
	purple_http_request_unref(request);
	
	g_string_free(postdata, TRUE);
	g_free(pie);
	g_free(etm);
	
	purple_connection_update_progress(sa->pc, _("Authenticating"), 2, 4);
}

void
skypeweb_begin_web_login(SkypeWebAccount *sa)
{
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login?method=skype&client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com";
	
	purple_http_get(sa->pc, skypeweb_login_got_pie, sa, login_url);
	
	purple_connection_set_state(sa->pc, PURPLE_CONNECTION_CONNECTING);
	purple_connection_update_progress(sa->pc, _("Connecting"), 1, 4);
}

static void
skypeweb_login_got_t(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	SkypeWebAccount *sa = user_data;
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login/microsoft";
	PurpleHttpRequest *request;
	GString *postdata;
	gchar *magic_t_value; // T is for tasty
	int tmplen;
	const gchar *data;
	gsize len;
	
	data = purple_http_response_get_data(response, &len);
	
	// <input type="hidden" name="t" id="t" value="...">
	magic_t_value = skypeweb_string_get_chunk(data, len, "=\"t\" value=\"", "\"");
	if (!magic_t_value) {
		//No Magic T????  Maybe it be the mighty 2fa-beast
		
		if (FALSE)
		/*if (g_strnstr(data, len, "Set-Cookie: LOpt=0;"))*/ {
			//XX - Would this be better retrieved with JSON decoding the "var ServerData = {...}" code?
			//     <script type="text/javascript">var ServerData = {...};</script>
			gchar *session_state = skypeweb_string_get_chunk(data, len, ":'https://login.live.com/GetSessionState.srf?", "',");
			if (session_state) {
				//These two appear to have different object keys each request :(
				/*
				gchar *PPFT = skypeweb_string_get_chunk(data, len, ",sFT:'", "',");
				gchar *SLK = skypeweb_string_get_chunk(data, len, ",aB:'", "',");
				gchar *ppauth_cookie = skypeweb_string_get_chunk(data, len, "Set-Cookie: PPAuth=", ";");
				gchar *mspok_cookie = skypeweb_string_get_chunk(data, len, "Set-Cookie: MSPOK=", "; domain=");
				*/
				
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

	tmplen = postdata->len;
	if (postdata->len > INT_MAX) tmplen = INT_MAX;

	// post the t to https://login.skype.com/login/oauth?client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com
	
	request = purple_http_request_new(login_url);
	purple_http_request_set_method(request, "POST");
	purple_http_request_set_cookie_jar(request, sa->cookie_jar);
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
	purple_http_request_header_set(request, "Accept", "*/*");
	purple_http_request_header_set(request, "BehaviorOverride", "redirectAs404");
	purple_http_request_set_contents(request, postdata->str, tmplen);
	purple_http_request_set_max_redirects(request, 0);
	purple_http_request(sa->pc, request, skypeweb_login_did_auth, sa);
	purple_http_request_unref(request);
	
	g_string_free(postdata, TRUE);
	g_free(magic_t_value);
	
	purple_connection_update_progress(sa->pc, _("Verifying"), 3, 4);
}

static void
skypeweb_login_got_ppft(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	SkypeWebAccount *sa = user_data;
	const gchar *live_login_url = "https://login.live.com" "/ppsecure/post.srf?wa=wsignin1.0&wp=MBI_SSL&wreply=https%3A%2F%2Flw.skype.com%2Flogin%2Foauth%2Fproxy%3Fsite_name%3Dlw.skype.com";
	gchar *cktst_cookie;
	gchar *ppft;
	GString *postdata;
	PurpleHttpRequest *request;
	int tmplen;
	const gchar *data;
	gsize len;

	data = purple_http_response_get_data(response, &len);
	
	// <input type="hidden" name="PPFT" id="i0327" value="..."/>
	ppft = skypeweb_string_get_chunk(data, len, "name=\"PPFT\" id=\"i0327\" value=\"", "\"");
	if (!ppft) {
		purple_connection_error(sa->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Failed getting PPFT value"));
		return;
	}
	// CkTst=G + timestamp   e.g. G1422309314913
	cktst_cookie = g_strdup_printf("G%" G_GINT64_FORMAT, skypeweb_get_js_time());
	purple_http_cookie_jar_set(sa->cookie_jar, "CkTst", cktst_cookie);
	
	// postdata: login={username}&passwd={password}&PPFT={ppft value}
	postdata = g_string_new("");
	g_string_append_printf(postdata, "login=%s&", purple_url_encode(purple_account_get_username(sa->account)));
	g_string_append_printf(postdata, "passwd=%s&", purple_url_encode(purple_connection_get_password(sa->pc)));
	g_string_append_printf(postdata, "PPFT=%s&", purple_url_encode(ppft));
	g_string_append(postdata, "loginoptions=3&");

	tmplen = postdata->len;
	if (postdata->len > INT_MAX) tmplen = INT_MAX;

	// POST to https://login.live.com/ppsecure/post.srf?wa=wsignin1.0&wreply=https%3A%2F%2Fsecure.skype.com%2Flogin%2Foauth%2Fproxy%3Fclient_id%3D578134%26redirect_uri%3Dhttps%253A%252F%252Fweb.skype.com
	
	request = purple_http_request_new(live_login_url);
	purple_http_request_set_method(request, "POST");
	purple_http_request_set_cookie_jar(request, sa->cookie_jar);
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
	purple_http_request_header_set(request, "Accept", "*/*");
	purple_http_request_set_contents(request, postdata->str, tmplen);
	purple_http_request(sa->pc, request, skypeweb_login_got_t, sa);
	purple_http_request_unref(request);
	
	g_string_free(postdata, TRUE);
	
	g_free(cktst_cookie);
	g_free(ppft);
	
	purple_connection_update_progress(sa->pc, _("Authenticating"), 2, 4);
}

void
skypeweb_begin_oauth_login(SkypeWebAccount *sa)
{
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login/oauth/microsoft?client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com";
	PurpleHttpRequest *request;
	
	request = purple_http_request_new(login_url);
	purple_http_request_set_cookie_jar(request, sa->cookie_jar);
	purple_http_request(sa->pc, request, skypeweb_login_got_ppft, sa);
	purple_http_request_unref(request);
	
	purple_connection_set_state(sa->pc, PURPLE_CONNECTION_CONNECTING);
	purple_connection_update_progress(sa->pc, _("Connecting"), 1, 4);
}

void
skypeweb_logout(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_LOGIN_HOST, "/logout", NULL, NULL, NULL, TRUE);
}



void
skypeweb_refresh_token_login(SkypeWebAccount *sa)
{
	PurpleAccount *account = sa->account;
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login?client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com";
	PurpleHttpRequest *request;
	
	request = purple_http_request_new(login_url);
	purple_http_request_set_method(request, "GET");
	purple_http_request_set_keepalive_pool(request, sa->keepalive_pool);
	purple_http_request_header_set(request, "Accept", "*/*");
	purple_http_request_header_set(request, "BehaviorOverride", "redirectAs404");
	purple_http_request_header_set_printf(request, "Cookie", "refresh-token=%s", purple_account_get_string(account, "refresh-token", ""));
	purple_http_request(sa->pc, request, skypeweb_login_did_auth, sa);
	purple_http_request_unref(request);
	
	purple_connection_update_progress(sa->pc, _("Authenticating"), 2, 4);
}
