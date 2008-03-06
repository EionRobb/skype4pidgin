#include <glib.h>
#include <debug.h>

typedef struct
{
	PurpleDebugLevel level;
	char *category;
	char *message;
} SkypeDebugWrapper;

void
skype_debug_vargs(PurpleDebugLevel level, const char *category,
				 const char *format, va_list args);
void skype_debug(PurpleDebugLevel level, const gchar *category,
				const gchar *format, ...);

void skype_debug_info(const gchar *category, const gchar *format, ...);
void skype_debug_warning(const gchar *category, const gchar *format, ...);
void skype_debug_error(const gchar *category, const gchar *format, ...);

static gboolean skype_debug_cb(SkypeDebugWrapper *wrapper);

void
skype_debug_info(const gchar *category, const gchar *format, ...)
{
	va_list args;

	va_start(args, format);
	skype_debug_vargs(PURPLE_DEBUG_INFO, category, format, args);
	va_end(args);
	
}

void
skype_debug_warning(const gchar *category, const gchar *format, ...)
{
	va_list args;

	va_start(args, format);
	skype_debug_vargs(PURPLE_DEBUG_WARNING, category, format, args);
	va_end(args);
}

void
skype_debug_error(const gchar *category, const gchar *format, ...)
{
	va_list args;

	va_start(args, format);
	skype_debug_vargs(PURPLE_DEBUG_ERROR, category, format, args);
	va_end(args);
}

void
skype_debug(PurpleDebugLevel level, const char *category,
				const char *format, ...)
{
	va_list args;

	va_start(args, format);
	skype_debug_vargs(level, category, format, args);
	va_end(args);
}

void
skype_debug_vargs(PurpleDebugLevel level, const char *category,
				 const char *format, va_list args)
{
	SkypeDebugWrapper *wrapper;
	
	if (purple_eventloop_get_ui_ops() == NULL)
	{
		return;
	}
	
	wrapper = g_new(SkypeDebugWrapper, 1);
	wrapper->level = level;
	wrapper->category = g_strdup(category);
	wrapper->message = g_strdup_vprintf(format, args);
	
	purple_timeout_add(1, (GSourceFunc) skype_debug_cb, (gpointer)wrapper);
}

static gboolean
skype_debug_cb(SkypeDebugWrapper *wrapper)
{
	if (wrapper != NULL)
	{
		purple_debug(wrapper->level, wrapper->category, wrapper->message);
		g_free(wrapper);
	}
	return FALSE;
}
