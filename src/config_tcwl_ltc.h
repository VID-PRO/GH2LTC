#pragma once
#include "config.h"
#define TCWL_LTC 1
#undef MAX7219_ENABLE
#define MAX7219_ENABLE 0

// No TC358743 on TC-WL-LTC — don't drive GPIO 4 as reset output
#undef TC_RESET_PIN
#define TC_RESET_PIN          -1

// Buttons on TC-WL-LTC PCB
#undef BTN_OK_PIN
#define BTN_OK_PIN            4
#undef BTN_CANCEL_PIN
#define BTN_CANCEL_PIN        5

// RTC optional — probed at boot, skipped if not found

// I2C on GPIO 6 (SDA), 7 (SCL) — XIAO ESP32-C3 default I2C pins (D4/D5)
// Shared bus for OLED + RTC
#undef TC_I2C_SDA_PIN
#define TC_I2C_SDA_PIN       6
#undef TC_I2C_SCL_PIN
#define TC_I2C_SCL_PIN       7
#undef RTC_I2C_SDA_PIN
#define RTC_I2C_SDA_PIN      6
#undef RTC_I2C_SCL_PIN
#define RTC_I2C_SCL_PIN      7

// LTC output on GPIO 20 (D6), LTC input on GPIO 21 (D7) — per TC-WL-LTC PCB
#undef LTC_OUT_PIN
#define LTC_OUT_PIN          20
#undef LTC_IN_PIN
#define LTC_IN_PIN           21

// XIAO ESP32-C3: D0 = GPIO 2 (ADC1_CH2), 200k:200k divider
#undef BAT_ADC_PIN
#define BAT_ADC_PIN           2
