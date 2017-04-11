#ifndef _IMAGE_H_
#define _IMAGE_H_

#include "imgstore.h"

#define PurpleImage  PurpleStoredImage
#define purple_image_new_from_file(p, e)  purple_imgstore_new_from_file(p)
#define purple_image_new_from_data(d, l)  purple_imgstore_add(d, l, NULL)
#define purple_image_get_path             purple_imgstore_get_filename
#define purple_image_get_size             purple_imgstore_get_size
#define purple_image_get_data             purple_imgstore_get_data
#define purple_image_get_extension        purple_imgstore_get_extension

#endif /* _IMAGE_H_ */
