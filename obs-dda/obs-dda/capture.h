#pragma once

#ifdef __cplusplus
extern "C" 
{
#endif

typedef struct _capture_bitmap
{
	int width;
	int height;
	char* buffer;
} capture_bitmap;

int capture_init();
capture_bitmap* capture_get_frame();

#ifdef __cplusplus
}
#endif
