#ifndef _CIRCULARBUFFER_H_
#define _CIRCULARBUFFER_H_

#include "circbuffer.h"

#define PurpleCircularBuffer                    PurpleCircBuffer
#define purple_circular_buffer_new              purple_circ_buffer_new
#define purple_circular_buffer_destroy          purple_circ_buffer_destroy
#define purple_circular_buffer_append           purple_circ_buffer_append
#define purple_circular_buffer_get_max_read     purple_circ_buffer_get_max_read
#define purple_circular_buffer_mark_read        purple_circ_buffer_mark_read
#define purple_circular_buffer_get_output(buf)  ((const gchar *) (buf)->outptr)
#define purple_circular_buffer_get_used(buf)    ((buf)->bufused)

#endif /*_CIRCULARBUFFER_H_*/
