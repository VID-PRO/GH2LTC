#pragma once
#include "config.h"
#define TCWL_LTC 1
#define TCWL_CLAP 1

// TC-WL-CLAP: WiFi + BLE client + LED matrix + OLED
#define MATRIX_ENABLED_DEFAULT 1

// OLED on shared I2C bus (default SDA=GPIO4, SCL=GPIO5 — free on CLAP)
#define OLED_ENABLE 1

// No physical buttons on CLAP
#undef BTN_UP_PIN
#undef BTN_DOWN_PIN
#undef BTN_OK_PIN
#undef BTN_CANCEL_PIN
#define BTN_UP_PIN       -1
#define BTN_DOWN_PIN     -1
#define BTN_OK_PIN       -1
#define BTN_CANCEL_PIN   -1
