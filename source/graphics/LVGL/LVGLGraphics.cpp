#include "graphics/LVGL/LVGLGraphics.h"
#include "assert.h"
#include "util/ILog.h"
#include <cstdio>

LVGLGraphics::LVGLGraphics(uint16_t width, uint16_t height) : screenWidth(width), screenHeight(height) {}

void LVGLGraphics::init(void)
{
    ILOG_DEBUG("LV init...");
#if LV_USE_LOG
    lv_log_register_print_cb(lv_debug);
#endif
    lv_init();

#if defined(MUI_LVGL_FORMAT_REPORT)
    std::fprintf(stderr,
                 "LVGL draw formats: RGB565=%d RGB565A8=%d A8=%d ARGB8888=%d XRGB8888=%d RGB888=%d\n",
                 (int)LV_DRAW_SW_SUPPORT_RGB565,
                 (int)LV_DRAW_SW_SUPPORT_RGB565A8,
                 (int)LV_DRAW_SW_SUPPORT_A8,
                 (int)LV_DRAW_SW_SUPPORT_ARGB8888,
                 (int)LV_DRAW_SW_SUPPORT_XRGB8888,
                 (int)LV_DRAW_SW_SUPPORT_RGB888);
#endif

#if LV_USE_LOG
    lv_log_register_print_cb(lv_debug);
#endif
}

// debugging callback
void LVGLGraphics::lv_debug(lv_log_level_t level, const char *buf)
{
    switch (level) {
    case LV_LOG_LEVEL_TRACE: {
        ILOG_DEBUG("%s", buf);
        break;
    }
    case LV_LOG_LEVEL_INFO: {
        ILOG_INFO("%s", buf);
        break;
    }
    case LV_LOG_LEVEL_WARN: {
        ILOG_WARN("%s", buf);
        break;
    }
    case LV_LOG_LEVEL_ERROR: {
        ILOG_ERROR("%s", buf);
        break;
    }
    default:
        ILOG_DEBUG("%s", buf);
        break;
    }
}
