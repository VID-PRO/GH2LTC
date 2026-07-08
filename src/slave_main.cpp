#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "ltc_encoder.h"
#include "ble_timecode.h"
#if OLED_ENABLE
#include "oled_display.h"
#endif
#if MAX7219_ENABLE
#include "max7219_display.h"
#endif
#if WEBUI_ENABLE
#include "webui.h"
#endif

// ---------------------------------------------------------------------------
// Objects
// ---------------------------------------------------------------------------
static LtcEncoder ltc(LTC_OUT_PIN, LTC_FPS, LTC_DROP_FRAME);

#if OLED_ENABLE
static OledDisplay oled;
#endif

#if MAX7219_ENABLE
static Max7219Display mx7219(MAX7219_DIN_PIN, MAX7219_CS_PIN, MAX7219_CLK_PIN, MAX7219_NUM_MODULES);
#endif

#if WEBUI_ENABLE
static WebUI webui;
#endif

static char tcStr[16];

// ---------------------------------------------------------------------------
// BLE timecode callback — called when a new timecode arrives from master
// ---------------------------------------------------------------------------
static void onBleTimecode(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
    ltc.setTime(hh, mm, ss, ff);
    ltc.setDd(dd);
}

// ---------------------------------------------------------------------------
// Helper: format timecode string for OLED
// ---------------------------------------------------------------------------
static void fmtTcStr(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
    snprintf(tcStr, sizeof(tcStr), "%02u:%02u:%02u:%02u", hh, mm, ss, ff);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("\n=== TC-LTC-SLAVE ==="));

    Wire.begin(TC_I2C_SDA_PIN, TC_I2C_SCL_PIN);

    // BLE client init
    Serial.print(F("BLE client init... "));
    bleTimecodeInit();
    bleTimecodeSetCallback(onBleTimecode);
    Serial.println(F("done"));

    // LTC encoder
    ltc.begin();
    ltc.setTime(0, 0, 0, 0);
    Serial.println(F("LTC encoder started"));

#if OLED_ENABLE
    oled.begin();
    Serial.println(F("OLED started"));
#endif

#if MAX7219_ENABLE
    mx7219.begin();
    mx7219.showText("SLAVE");
    delay(2000);
    Serial.println(F("MAX7219 started"));
#endif

#if WEBUI_ENABLE
    Serial.print(F("Starting WiFi AP... "));
    webui.begin(WEBUI_AP_SSID, WEBUI_AP_PASSWORD,
                WEBUI_STA_SSID, WEBUI_STA_PASSWORD);
#else
    // Simple WiFi for BLE + STA connectivity
    WiFi.mode(WIFI_STA);
    WiFi.begin(WEBUI_STA_SSID, WEBUI_STA_PASSWORD);
#endif

    Serial.println(F("Ready. Scanning for master..."));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
#if WEBUI_ENABLE
    webui.handleClient();
#endif

    // Poll BLE for master connection
    bleTimecodePoll();

    // Frame tick
    uint16_t frames = ltc.framesCompleted();
    while (frames--) ltc.tick();

    // Update displays
#if OLED_ENABLE
    fmtTcStr(ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
    oled.update(tcStr, ltc.fps(), bleTimecodeConnected());
#endif

#if MAX7219_ENABLE
#if WEBUI_ENABLE
    if (webui.matrixEnabled()) {
        mx7219.showTimecode(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
    }
#else
    mx7219.showTimecode(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
#endif
#endif

#if WEBUI_ENABLE
    webui.update(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff(),
                 ltc.fps(), ltc.dropFrame(), bleTimecodeConnected(),
                 bleTimecodeConnected() ? "BLE" : "SCAN");
#endif
}
