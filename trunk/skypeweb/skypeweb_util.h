#include "libskypeweb.h"

gchar *skypeweb_string_get_chunk(const gchar *haystack, gsize len, const gchar *start, const gchar *end);

gchar *skypeweb_jsonobj_to_string(JsonObject *jsonobj);

const gchar *skypeweb_contact_url_to_name(const gchar *url);
const gchar *skypeweb_thread_url_to_name(const gchar *url);

gchar *skypeweb_hmac_sha256(gchar *input);

gint64 skypeweb_get_js_time();