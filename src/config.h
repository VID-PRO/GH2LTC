#pragma once

// ---------------------------------------------------------------------------
// Pin & hardware configuration — ESP32-C3 Super Mini
// ---------------------------------------------------------------------------

// I2C pins to the TC358743 HDMI receiver board (single bus, shared with OLED & RTC)
#define TC_I2C_SDA_PIN   4
#define TC_I2C_SCL_PIN   5
#define TC_I2C_ADDR      0x0F   // default 7-bit address on most TC358743 breakout boards

// TC358743 hardware reset pin (set to -1 if your board doesn't expose RESET)
#define TC_RESET_PIN     -1

// LTC audio output pin (goes to your RC filter -> 3.5mm TRS -> LTC input)
#define LTC_OUT_PIN      6

// Onboard LED used as a simple "locked / not locked" status indicator
// ESP32-C3 Super Mini typically has the LED on GPIO8
#define STATUS_LED_PIN   7           // do NOT use GPIO8/9 (USB D-/D+) — they share PLL with WiFi PHY

// ---------------------------------------------------------------------------
// Frame rate auto-detection
// ---------------------------------------------------------------------------
#ifndef FPS_AUTO_DETECT
#define FPS_AUTO_DETECT     1
#endif

// ---------------------------------------------------------------------------
// OLED display configuration
// ---------------------------------------------------------------------------
// 128x64 SSD1306 OLED on the shared I2C bus.
#define OLED_ENABLE         1
#define OLED_I2C_ADDR       0x3C

// ---------------------------------------------------------------------------
// RTC (Real-Time Clock) configuration
// ---------------------------------------------------------------------------
// DS3231 RTC on the shared I2C bus (same SDA/SCL as TC358743 + OLED — all at
// different addresses: 0x0F, 0x3C, 0x68 — no conflict).
#define RTC_ENABLE          1
#define RTC_I2C_SDA_PIN     4   // same bus as TC358743 / OLED (only one I2C on ESP32-C3)
#define RTC_I2C_SCL_PIN     5
#define RTC_I2C_ADDR        0x68

// ---------------------------------------------------------------------------
// LTC frame rate fallback defaults
// ---------------------------------------------------------------------------
#ifndef LTC_FPS
#define LTC_FPS 25
#endif

#ifndef LTC_DROP_FRAME
#define LTC_DROP_FRAME 0
#endif

// ---------------------------------------------------------------------------
// MAX7219 8x8 LED matrix display configuration (optional — set to 0 to skip)
// 8 modules daisy-chained = 64x8 pixels. Software SPI (no hardware SPI on these pins).
// Pins: see schematic for wiring on ESP32-C3 Super Mini.
#define MAX7219_ENABLE       1
#define MAX7219_DIN_PIN      2
#define MAX7219_CS_PIN       3
#define MAX7219_CLK_PIN      10
#define MAX7219_NUM_MODULES  8

// ---------------------------------------------------------------------------
// Web UI (WiFi AP + optional STA) configuration
// ---------------------------------------------------------------------------
#define WEBUI_ENABLE        1
#define WEBUI_AP_SSID       "TC-LTC-GENERATOR"
#define WEBUI_AP_PASSWORD   nullptr         // nullptr = open network

// Optional: connect to an existing WiFi network (STA mode) so the web UI
// is also reachable on your LAN.  Leave SSID empty ("") to skip STA.
#define WEBUI_STA_SSID      ""
#define WEBUI_STA_PASSWORD  ""

// ---------------------------------------------------------------------------
// Operating mode
// ---------------------------------------------------------------------------
#define REVERSE_ENGINEER_MODE 1

// ---------------------------------------------------------------------------
// Slave mode overrides (BLE_SLAVE defined in build_flags for slave env)
// ---------------------------------------------------------------------------
#ifdef BLE_SLAVE
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
#endif
