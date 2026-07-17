#pragma once
#include "config.h"
#define TCWL_HDMI 1

// ESP32-P4-WIFI6 CSI connector I2C uses GPIO7(SDA)/GPIO8(SCL)
// (default config.h has TC_I2C_SDA_PIN=4, TC_I2C_SCL_PIN=5 which is for
// the ESP32-C3 Super Mini's separate I2C bus to OLED/RTC)
#undef TC_I2C_SDA_PIN
#undef TC_I2C_SCL_PIN
#define TC_I2C_SDA_PIN   7
#define TC_I2C_SCL_PIN   8

// TC358743 hardware reset pin (GPIO 4 — wire to Geekworm C790 CE pin)
// CE is pin 1 on the CSI ribbon cable; pull low for reset, high for normal operation.
#undef TC_RESET_PIN
#define TC_RESET_PIN      4

// TC-WL-HDMI has no LED matrix (different GPIO layout)
#undef MAX7219_ENABLE
#define MAX7219_ENABLE        0

// Reverse‑engineer mode (dump GH5 InfoFrame packets over serial to decode byte layout)
#define REVERSE_ENGINEER_MODE 0

// Button pins — GPIO 8 is used for I2C SCL on HDMI, so UP is moved to GPIO 10.
// GPIO 2, 3, 10 are free because MAX7219 is disabled.
#undef BTN_UP_PIN
#undef BTN_DOWN_PIN
#undef BTN_OK_PIN
#undef BTN_CANCEL_PIN
#define BTN_UP_PIN       10
#define BTN_DOWN_PIN      9
#define BTN_OK_PIN        2
#define BTN_CANCEL_PIN    3

// Battery ADC — GPIO 20 (ADC1_CH4) on 40-pin header pin 10.
// Wire a voltage divider from battery to this pin.
#undef BAT_ADC_PIN
#define BAT_ADC_PIN           20

// TC358743 reference clock crystal on this module is 27 MHz (not 42 MHz)
#define TC_REFCLK_HZ          27000000u

// RTC disabled on P4 — RTC I2C uses GPIO 4 (SDA) + 5 (SCL), but GPIO 4 is
// also TC_RESET_PIN (CE).  rtc.begin() toggles GPIO 4 as I2C data, which
// glitches the CE line and resets the TC358743.  No RTC on this board.
#undef RTC_ENABLE
#define RTC_ENABLE            1

