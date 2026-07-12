#pragma once
#include "config.h"
#define BLE_MASTER 1

// ESP32-P4-WIFI6 CSI connector I2C uses GPIO7(SDA)/GPIO8(SCL)
// (default config.h has TC_I2C_SDA_PIN=4, TC_I2C_SCL_PIN=5 which is for
// the ESP32-C3 Super Mini's separate I2C bus to OLED/RTC)
#undef TC_I2C_SDA_PIN
#undef TC_I2C_SCL_PIN
#define TC_I2C_SDA_PIN   7
#define TC_I2C_SCL_PIN   8

// Master has no LED matrix (different GPIO layout)
#undef MAX7219_ENABLE
#define MAX7219_ENABLE        0

// Reverse‑engineer mode (dump GH5 InfoFrame packets over serial to decode byte layout)
#define REVERSE_ENGINEER_MODE 0


