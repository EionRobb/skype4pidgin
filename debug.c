/*
 * Skype plugin for libpurple/Pidgin/Adium
 * Written by: Eion Robb <eionrobb@gmail.com>
 *
 * This plugin uses the Skype API to show your contacts in libpurple, and send/receive
 * chat messages.
 * It requires the Skype program to be running.
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
	gchar *message;
	
	if (purple_eventloop_get_ui_ops() == NULL)
	{
		return;
	}
	
	wrapper = g_new(SkypeDebugWrapper, 1);
	wrapper->level = level;
	wrapper->category = g_strdup(category);
	message = g_strdup_vprintf(format, args);
	wrapper->message = purple_strreplace(message, "%", "%%");
	g_free(message);
	
	purple_timeout_add(1, (GSourceFunc) skype_debug_cb, (gpointer)wrapper);
}

static gboolean
skype_debug_cb(SkypeDebugWrapper *wrapper)
{
	if (wrapper != NULL)
	{
		purple_debug(wrapper->level, wrapper->category, "%s", wrapper->message);
		g_free(wrapper->category);
		g_free(wrapper->message);
		g_free(wrapper);
	}
	return FALSE;
}
