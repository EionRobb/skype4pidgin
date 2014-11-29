
#ifndef SKYPEWEB_CONNECTION_H
#define SKYPEWEB_CONNECTION_H

#include "libskypeweb.h"

typedef void (*SkypeWebProxyCallbackFunc)(SkypeWebAccount *sa, JsonNode *node, gpointer user_data);
typedef void (*SkypeWebProxyCallbackErrorFunc)(SkypeWebAccount *sa, const gchar *data, gssize data_len, gpointer user_data);

/*
 * This is a bitmask.
 */
typedef enum
{
	SKYPEWEB_METHOD_GET    = 0x0001,
	SKYPEWEB_METHOD_POST   = 0x0002,
	SKYPEWEB_METHOD_PUT    = 0x0004,
	SKYPEWEB_METHOD_DELETE = 0x0008,
	SKYPEWEB_METHOD_SSL    = 0x1000,
} SkypeWebMethod;

typedef struct _SkypeWebConnection SkypeWebConnection;
struct _SkypeWebConnection {
	SkypeWebAccount *sa;
	SkypeWebMethod method;
	gchar *hostname;
	gchar *url;
	GString *request;
	SkypeWebProxyCallbackFunc callback;
	gpointer user_data;
	char *rx_buf;
	size_t rx_len;
	PurpleProxyConnectData *connect_data;
	PurpleSslConnection *ssl_conn;
	int fd;
	guint input_watcher;
	gboolean connection_keepalive;
	time_t request_time;
	guint retry_count;
	guint timeout_watcher;
	SkypeWebProxyCallbackErrorFunc error_callback;
};

void skypeweb_connection_destroy(SkypeWebConnection *skypewebcon);
void skypeweb_connection_close(SkypeWebConnection *skypewebcon);
SkypeWebConnection *skypeweb_post_or_get(SkypeWebAccount *sa, SkypeWebMethod method,
		const gchar *host, const gchar *url, const gchar *postdata,
		SkypeWebProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive);
gchar *skypeweb_cookies_to_string(SkypeWebAccount *sa);



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

#endif /* SKYPEWEB_CONNECTION_H */
