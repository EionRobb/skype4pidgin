#ifndef _IMAGE_STORE_H_
#define _IMAGE_STORE_H_

#include "imgstore.h"
#include "image.h"


#define purple_image_store_add(img)  purple_imgstore_add_with_id( \
	g_memdup(purple_imgstore_get_data(img), purple_imgstore_get_size(img)), \
	purple_imgstore_get_size(img), purple_imgstore_get_filename(img))
#define purple_image_store_get       purple_imgstore_find_by_id


#endif /*_IMAGE_STORE_H_*/
