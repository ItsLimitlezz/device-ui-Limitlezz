#include "graphics/map/MapTileSettings.h"
#include "lv_conf.h"
#include "lvgl.h"

uint8_t MapTileSettings::zoomLevel = 13;   // current zoomLevel
uint8_t MapTileSettings::zoomDefault = 13; // default for initial or home position
uint16_t MapTileSettings::tileSize = 256;
uint32_t MapTileSettings::cacheSize = 50 * 1024;    // LV_FS_CACHE_FROM_BUFFER
float MapTileSettings::defaultLat = 51.5003646652f; // @theBigBentern
float MapTileSettings::defaultLon = -0.1214328476f;
#if defined(ARCH_PORTDUINO)
// Portduino FS paths must be absolute from the VFS root. Relative paths ("./maps")
// will fail with "does not start with /".
char MapTileSettings::prefix[10] = "/maps";   // default map tile directory for native sim
#else
char MapTileSettings::prefix[10] = "/maps";   // default map tile directory
#endif
char MapTileSettings::tileStyle[20] = "";     // { osm/, atlas/, atlas-mobile/, ...}
#ifndef MUI_MAP_TILE_FORMAT_DEFAULT
#define MUI_MAP_TILE_FORMAT_DEFAULT "png"
#endif

char MapTileSettings::tileFormat[10] = MUI_MAP_TILE_FORMAT_DEFAULT; // e.g. "png", "jpg", "bin"
bool MapTileSettings::debug = false;