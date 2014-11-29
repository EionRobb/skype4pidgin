#include "skypeweb_login.h"
#include "skypeweb_util.h"

static void
skypeweb_login_did_auth(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	gchar *refresh_token;
	SkypeWebAccount *sa = user_data;
	
	if (url_text == NULL) {
		url_text = url_data->webdata;
		len = url_data->data_len;
	}
	
	refresh_token = skypeweb_string_get_chunk(url_text, len, "Set-Cookie: refresh-token=", ";");
	if (refresh_token == NULL) {
		purple_debug_info("skypeweb", "login response was %s\r\n", url_text);
		purple_connection_error (sa->pc,
								PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
								_("Failed getting Skype Token"));
		return;
	}
	
	sa->skype_token = refresh_token;
	
	skypeweb_do_all_the_things(sa);
}

static void
skypeweb_login_got_pie(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	SkypeWebAccount *sa = user_data;
	PurpleAccount *account = sa->account;
	gchar *pie;
	gchar *etm;
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login?message=signin_continue";
	GString *postdata;
	gchar *request;
	struct timezone tz;
	guint tzhours, tzminutes;
	PurpleUtilFetchUrlData *requestdata;
	
	gettimeofday(NULL, &tz);
	tzminutes = tz.tz_minuteswest;
	if (tzminutes < 0) tzminutes = -tzminutes;
	tzhours = tzminutes / 60;
	tzminutes -= tzhours * 60;
	
	pie = skypeweb_string_get_chunk(url_text, len, "=\"pie\" value=\"", "\"");
	if (!pie) {
		return;
	}
	
	etm = skypeweb_string_get_chunk(url_text, len, "=\"etm\" value=\"", "\"");
	if (!etm) {
		return;
	}
	
	
	postdata = g_string_new("");
	g_string_append_printf(postdata, "username=%s&", purple_url_encode(purple_account_get_username(account)));
	g_string_append_printf(postdata, "password=%s&", purple_url_encode(purple_account_get_password(account)));
	g_string_append_printf(postdata, "timezone_field=%c|%d|%d&", (tz.tz_minuteswest < 0 ? '+' : '-'), tzhours, tzminutes);
	g_string_append_printf(postdata, "pie=%s&", purple_url_encode(pie));
	g_string_append_printf(postdata, "etm=%s&", purple_url_encode(etm));
	g_string_append_printf(postdata, "js_time=%d&", time(NULL));
	g_string_append(postdata, "return_url=https://web.skype.com/");
	
	request = g_strdup_printf("POST /login?message=signin_continue HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: */*\r\n"
			"BehaviorOverride: redirectAs404\r\n"
			"Host: " SKYPEWEB_LOGIN_HOST "\r\n"
			"Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n"
			"Content-Length: %" G_GSIZE_FORMAT "\r\n\r\n%s",
			strlen(postdata->str), postdata->str);
	
#if PURPLE_VERSION_CHECK(3, 0, 0)
	requestdata = purple_util_fetch_url_request(sa->account, login_url, TRUE, NULL, FALSE, request, TRUE, -1, skypeweb_login_did_auth, sa);
#else
	requestdata = purple_util_fetch_url_request(login_url, TRUE, NULL, FALSE, request, TRUE, skypeweb_login_did_auth, sa);
#endif
	requestdata->num_times_redirected = 10; /* Prevent following redirects */

	g_string_free(postdata, TRUE);
	g_free(request);
}

void
skypeweb_begin_web_login(SkypeWebAccount *sa)
{
	const gchar *login_url = "https://" SKYPEWEB_LOGIN_HOST "/login";
	
#if PURPLE_VERSION_CHECK(3, 0, 0)
	purple_util_fetch_url_request(sa->account, login_url, TRUE, NULL, FALSE, NULL, FALSE, -1, skypeweb_login_got_pie, sa);
#else
	purple_util_fetch_url_request(login_url, TRUE, NULL, FALSE, NULL, FALSE, skypeweb_login_got_pie, sa);
#endif
	
	purple_connection_set_state(sa->pc, PURPLE_CONNECTING);
	purple_connection_update_progress(sa->pc, _("Connecting"), 1, 3);
}

void
skypeweb_logout(SkypeWebAccount *sa)
{
	skypeweb_post_or_get(sa, SKYPEWEB_METHOD_GET | SKYPEWEB_METHOD_SSL, SKYPEWEB_LOGIN_HOST, "/logout", NULL, NULL, NULL, TRUE);
}
