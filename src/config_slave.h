#pragma once

// Slave config — include common base, set BLE_SLAVE role, override shared settings
#include "config.h"
#define BLE_SLAVE 1

#undef TC_RESET_PIN
#undef RTC_ENABLE
#undef OLED_ENABLE
#undef FPS_AUTO_DETECT
#undef REVERSE_ENGINEER_MODE

#define TC_RESET_PIN          -1
#define RTC_ENABLE            0
#define OLED_ENABLE           0
#define FPS_AUTO_DETECT       0
#define REVERSE_ENGINEER_MODE 0

#undef WEBUI_AP_SSID
#define WEBUI_AP_SSID        "TC-LTC-SLAVE"
