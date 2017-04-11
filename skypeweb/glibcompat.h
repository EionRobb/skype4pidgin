#ifndef _GLIBCOMPAT_H_
#define _GLIBCOMPAT_H_

#if !GLIB_CHECK_VERSION(2, 32, 0)
#define g_hash_table_contains(hash_table, key) g_hash_table_lookup_extended(hash_table, key, NULL, NULL)
#endif /* 2.32.0 */


#if !GLIB_CHECK_VERSION(2, 28, 0)
gint64
g_get_real_time()
{
	GTimeVal val;
	
	g_get_current_time (&val);
	
	return (((gint64) val.tv_sec) * 1000000) + val.tv_usec;
}
#endif /* 2.28.0 */

#endif /*_GLIBCOMPAT_H_*/