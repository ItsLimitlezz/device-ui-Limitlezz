#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register a PNG decoder that decodes PNGs to RGB565 (no alpha) instead of ARGB8888.
 *
 * Intended for SD card images where ARGB8888 is too heavy.
 * Must be called after lv_init().
 */
void lv_lodepng_rgb565_init(void);

#ifdef __cplusplus
}
#endif
