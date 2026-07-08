#pragma once

// Dual-mode config — include common base and enable both BLE roles at runtime
// The role (master/slave) is selected at boot from NVS and can be changed via web UI.
#include "config.h"
#define BLE_MASTER 1
#define BLE_SLAVE 1
#define BLE_MODE_RUNTIME 1

// Override AP SSID to be mode-neutral
#undef WEBUI_AP_SSID
#define WEBUI_AP_SSID "TC-LTC-DUAL"
