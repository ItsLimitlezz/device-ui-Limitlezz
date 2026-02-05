#include "graphics/map/MapTileSettings.h"
#include "lv_conf.h"
#include "lvgl.h"

uint8_t MapTileSettings::zoomLevel = 13;   // current zoomLevel
uint8_t MapTileSettings::zoomDefault = 13; // default for initial or home position
uint16_t MapTileSettings::tileSize = 256;
uint32_t MapTileSettings::cacheSize = 50 * 1024;    // LV_FS_CACHE_FROM_BUFFER
float MapTileSettings::defaultLat = 51.5003646652f; // @theBigBentern
float MapTileSettings::defaultLon = -0.1214328476f;
char MapTileSettings::prefix[10] = "/maps";  // default map tile directory (fallback to /map if missing)
char MapTileSettings::tileStyle[20] = "";     // { osm/, atlas/, atlas-mobile/, ...}
char MapTileSettings::tileFormat[10] = "bin"; // use bin by default (RGB565 LVGL)
bool MapTileSettings::debug = false;
