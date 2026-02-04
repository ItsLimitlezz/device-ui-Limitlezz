#include "lv_lodepng_rgb565.h"

#if LV_USE_LODEPNG

// NOTE: PlatformIO typically adds the LVGL library directory itself to include paths.
// Use paths relative to the LVGL root (not "lvgl/") so this compiles in native-mui.
#include "src/draw/lv_image_decoder_private.h"
#include "src/core/lv_global.h"
#include "src/libs/lodepng/lodepng.h"

#include <stdlib.h>

#define DECODER_NAME "LODEPNG_RGB565"
#define image_cache_draw_buf_handlers &(LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers)

static lv_result_t decoder_info(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc, lv_image_header_t * header);
static lv_result_t decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);
static void decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);

static lv_draw_buf_t * decode_png_data_rgb565(const void * png_data, size_t png_data_size, unsigned * w, unsigned * h);

void lv_lodepng_rgb565_init(void)
{
    lv_image_decoder_t * dec = lv_image_decoder_create();
    lv_image_decoder_set_info_cb(dec, decoder_info);
    lv_image_decoder_set_open_cb(dec, decoder_open);
    lv_image_decoder_set_close_cb(dec, decoder_close);
    dec->name = DECODER_NAME;
}

static lv_result_t decoder_info(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc, lv_image_header_t * header)
{
    LV_UNUSED(decoder);

    if(dsc->src_type != LV_IMAGE_SRC_FILE && dsc->src_type != LV_IMAGE_SRC_VARIABLE) return LV_RESULT_INVALID;

    static const uint8_t magic[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    uint8_t buf[24];
    uint32_t * size;

    if(dsc->src_type == LV_IMAGE_SRC_FILE) {
        uint32_t rn;
        lv_fs_read(&dsc->file, buf, sizeof(buf), &rn);
        if(rn != sizeof(buf)) return LV_RESULT_INVALID;
        if(lv_memcmp(buf, magic, sizeof(magic)) != 0) return LV_RESULT_INVALID;
        size = (uint32_t *)&buf[16];
    }
    else {
        const lv_image_dsc_t * img_dsc = dsc->src;
        if(img_dsc->data_size < sizeof(magic)) return LV_RESULT_INVALID;
        if(lv_memcmp(img_dsc->data, magic, sizeof(magic)) != 0) return LV_RESULT_INVALID;
        size = ((uint32_t *)img_dsc->data) + 4;
    }

    header->cf = LV_COLOR_FORMAT_RGB565;
    header->w = (int32_t)((size[0] & 0xff000000) >> 24) + ((size[0] & 0x00ff0000) >> 8);
    header->h = (int32_t)((size[1] & 0xff000000) >> 24) + ((size[1] & 0x00ff0000) >> 8);

    return LV_RESULT_OK;
}

static lv_result_t decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);

    const uint8_t * png_data = NULL;
    size_t png_data_size = 0;

    if(dsc->src_type == LV_IMAGE_SRC_FILE) {
        const char * fn = dsc->src;
        unsigned error = lodepng_load_file((void *)&png_data, &png_data_size, fn);
        if(error) {
            if(png_data != NULL) lv_free((void *)png_data);
            return LV_RESULT_INVALID;
        }
    }
    else if(dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t * img_dsc = dsc->src;
        png_data = img_dsc->data;
        png_data_size = img_dsc->data_size;
    }
    else {
        return LV_RESULT_INVALID;
    }

    unsigned w = 0, h = 0;
    lv_draw_buf_t * decoded = decode_png_data_rgb565(png_data, png_data_size, &w, &h);

    if(dsc->src_type == LV_IMAGE_SRC_FILE) lv_free((void *)png_data);

    if(!decoded) return LV_RESULT_INVALID;

    lv_draw_buf_t * adjusted = lv_image_decoder_post_process(dsc, decoded);
    if(adjusted == NULL) {
        lv_draw_buf_destroy(decoded);
        return LV_RESULT_INVALID;
    }

    if(adjusted != decoded) {
        lv_draw_buf_destroy(decoded);
        decoded = adjusted;
    }

    dsc->decoded = decoded;

    if(dsc->args.no_cache) return LV_RESULT_OK;
    if(!lv_image_cache_is_enabled()) return LV_RESULT_OK;

    lv_image_cache_data_t search_key;
    search_key.src_type = dsc->src_type;
    search_key.src = dsc->src;
    search_key.slot.size = decoded->data_size;

    lv_cache_entry_t * entry = lv_image_decoder_add_to_cache(decoder, &search_key, decoded, NULL);
    if(entry == NULL) return LV_RESULT_INVALID;

    dsc->cache_entry = entry;
    return LV_RESULT_OK;
}

static void decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);
    if(dsc->args.no_cache || !lv_image_cache_is_enabled()) {
        lv_draw_buf_destroy((lv_draw_buf_t *)dsc->decoded);
    }
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | ((b & 0xF8u) >> 3));
}

static lv_draw_buf_t * decode_png_data_rgb565(const void * png_data, size_t png_data_size, unsigned * w, unsigned * h)
{
    unsigned png_width = 0;
    unsigned png_height = 0;
    unsigned char * rgba = NULL;

    unsigned error = lodepng_decode32(&rgba, &png_width, &png_height, png_data, png_data_size);
    if(error) {
        if(rgba) lodepng_free(rgba);
        return NULL;
    }

    lv_draw_buf_t * out = lv_draw_buf_create_ex(image_cache_draw_buf_handlers, png_width, png_height, LV_COLOR_FORMAT_RGB565,
                                               LV_STRIDE_AUTO);
    if(!out) {
        lodepng_free(rgba);
        return NULL;
    }

    uint16_t * dst = (uint16_t *)out->data;
    const uint8_t * src = (const uint8_t *)rgba;
    const uint32_t px_cnt = (uint32_t)png_width * (uint32_t)png_height;

    for(uint32_t i = 0; i < px_cnt; i++) {
        uint8_t r = src[i * 4 + 0];
        uint8_t g = src[i * 4 + 1];
        uint8_t b = src[i * 4 + 2];
        uint8_t a = src[i * 4 + 3];

        if(a != 255) {
            /* Drop alpha. Simple preblend against black to avoid bright halos. */
            r = (uint8_t)((r * (uint16_t)a) / 255u);
            g = (uint8_t)((g * (uint16_t)a) / 255u);
            b = (uint8_t)((b * (uint16_t)a) / 255u);
        }
        dst[i] = rgb565(r, g, b);
    }

    lodepng_free(rgba);
    if(w) *w = png_width;
    if(h) *h = png_height;
    return out;
}

#endif /* LV_USE_LODEPNG */
