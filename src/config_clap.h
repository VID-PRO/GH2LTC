#pragma once
#include "config.h"
#define BLE_SLAVE 1
#define BLE_CLAP 1

// Clap: WiFi + BLE slave + LED matrix only
#undef OLED_ENABLE
#define OLED_ENABLE 0
#undef RTC_ENABLE
#define RTC_ENABLE 0
#undef FPS_AUTO_DETECT
#define FPS_AUTO_DETECT 0
