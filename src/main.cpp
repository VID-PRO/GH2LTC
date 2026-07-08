#include <Arduino.h>
#include <esp_wifi.h>
#include "esp_system.h"
#include <Preferences.h>
#include "config.h"
#include "tc358743.h"
#include "tc358743_regs.h"
#include "ltc_encoder.h"
#include "panasonic_tc.h"
#if RTC_ENABLE
#include "ds3231.h"
#endif
#if OLED_ENABLE
#include "oled_display.h"
#endif
#if MAX7219_ENABLE
#include "max7219_display.h"
#endif
#if WEBUI_ENABLE
#include "webui.h"
#endif
#include "ble_timecode.h"

TC358743 tc(Wire, TC_I2C_ADDR);
LtcEncoder ltc(LTC_OUT_PIN, LTC_FPS, LTC_DROP_FRAME);

static char tcStr[13] = "00:00:00:00";

static void fmtTcStr(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
    snprintf(tcStr, sizeof(tcStr), "%02u:%02u:%02u:%02u", hh, mm, ss, ff);
}

#if OLED_ENABLE
static OledDisplay oled;
#endif

static bool tcPresent = false;

#if RTC_ENABLE
static DS3231 rtc(Wire, RTC_I2C_ADDR);
static bool rtcPresent = false;
static unsigned long lastRtcReadMs = 0;
static unsigned long lastRtcSyncMs = 0;
static uint8_t rtcHH = 0, rtcMM = 0, rtcSS = 0;
#endif

#if WEBUI_ENABLE
static WebUI webui;
#endif
#if MAX7219_ENABLE
static Max7219Display mx7219(MAX7219_DIN_PIN, MAX7219_CS_PIN, MAX7219_CLK_PIN, MAX7219_NUM_MODULES);
#endif
static char tcSource[8] = "FREE";

static const unsigned long FAST_POLL_MS = 8;
unsigned long lastFastPoll = 0;
unsigned long lastFramePoll = 0;
unsigned long framePollMs = 1000 / LTC_FPS;

// ---------------------------------------------------------------------------
// Frame rate auto-detection
// ---------------------------------------------------------------------------
#if FPS_AUTO_DETECT
static bool fpsDetected = false;
static unsigned long lastFrameUs = 0;
static unsigned long intervals[30];
static uint8_t intervalCount = 0;
static const uint8_t MIN_SAMPLES = 10;

static void resetFpsDetect() {
    fpsDetected = false;
    intervalCount = 0;
    lastFrameUs = 0;
}

static bool tryDetectFps() {
    if (intervalCount < MIN_SAMPLES) return false;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < intervalCount; i++) sum += intervals[i];
    uint32_t avg = sum / intervalCount;

    uint8_t fps;
    if      (avg >= 40833) fps = 24;
    else if (avg >= 36667) fps = 25;
    else if (avg >= 26667) fps = 30;
    else if (avg >= 18333) fps = 50;
    else                   fps = 60;

    bool df = LTC_DROP_FRAME;

    if (fps != LTC_FPS || df != LTC_DROP_FRAME) {
        ltc.setFps(fps, df);
        framePollMs = 1000 / fps;
    }

    Serial.print(F("Auto-detected "));
    Serial.print(fps);
    Serial.println(F(" fps"));
    fpsDetected = true;
    return true;
}
#endif

// ---------------------------------------------------------------------------
// Reverse-engineer mode helpers
// ---------------------------------------------------------------------------
#if REVERSE_ENGINEER_MODE
void dumpBuffer(const char *label, uint16_t reg, size_t len) {
    uint8_t buf[32];
    if (len > sizeof(buf)) len = sizeof(buf);
    tc.readBlock(reg, buf, len);
    Serial.print(label);
    Serial.print(": ");
    for (size_t i = 0; i < len; i++) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

void runReverseEngineerDump() {
    Serial.println(F("--- InfoFrame / packet dump ---"));
    Serial.print(F("Signal locked: "));
    Serial.println(tc.hasSignal() ? F("yes") : F("no"));
    Serial.print(F("HDMI mode: "));
    Serial.println(tc.isHdmiMode() ? F("yes (good, packets active)") : F("no (DVI - no packets!)"));

    dumpBuffer("AVI InfoFrame       ", PK_AVI_0HEAD, PK_AVI_LEN);
    dumpBuffer("Vendor-Specific IF  ", PK_VS_0HEAD,  PK_VS_LEN);
    dumpBuffer("Audio InfoFrame     ", PK_AUD_0HEAD, PK_AUD_LEN);
    dumpBuffer("Source Product IF   ", PK_SPD_0HEAD, PK_SPD_LEN);
    dumpBuffer("MPEG Source IF      ", PK_MS_0HEAD,  PK_MS_LEN);
    dumpBuffer("ISRC1               ", PK_ISRC1_0HEAD, PK_ISRC1_LEN);
    dumpBuffer("ISRC2               ", PK_ISRC2_0HEAD, PK_ISRC2_LEN);

    static const uint8_t typesToTry[] = {
        PACKET_TYPE_ACP, PACKET_TYPE_GAMUT_METADATA, PACKET_TYPE_DRM_IF
    };
    for (uint8_t t : typesToTry) {
        tc.selectPacketType(t);
        delay(5);
        char label[32];
        snprintf(label, sizeof(label), "ACP slot (type 0x%02X)", t);
        dumpBuffer(label, PK_ACP_0HEAD, PK_ACP_LEN);
    }

    Serial.println();
}
#endif

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
static void printResetReason() {
    const char *reason;
    switch (esp_reset_reason()) {
        case ESP_RST_UNKNOWN:    reason = "unknown";          break;
        case ESP_RST_POWERON:    reason = "power-on";         break;
        case ESP_RST_SW:         reason = "software reset";   break;
        case ESP_RST_PANIC:      reason = "panic (crash)";    break;
        case ESP_RST_INT_WDT:    reason = "interrupt WDT";    break;
        case ESP_RST_TASK_WDT:   reason = "task WDT";         break;
        case ESP_RST_WDT:        reason = "other watchdog";   break;
        case ESP_RST_BROWNOUT:   reason = "brownout";         break;
        case ESP_RST_SDIO:       reason = "SDIO";             break;
        default:                 reason = "?";                break;
    }
    Serial.print(F("Last reset: "));
    Serial.println(reason);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    printResetReason();

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    Serial.println(F("GH5 HDMI -> LTC box starting..."));
    Serial.println();

    // ---- config dump ----
    Serial.println(F("── CONFIG ──────────────────────────────"));
    Serial.print(F("  LTC_OUT_PIN       ")); Serial.println(LTC_OUT_PIN);
    Serial.print(F("  STATUS_LED_PIN    ")); Serial.println(STATUS_LED_PIN);
    Serial.print(F("  LTC_FPS           ")); Serial.println(LTC_FPS);
    Serial.print(F("  LTC_DROP_FRAME    ")); Serial.println(LTC_DROP_FRAME);
    Serial.print(F("  FPS_AUTO_DETECT   ")); Serial.println(FPS_AUTO_DETECT);
    Serial.print(F("  REVERSE_ENGINEER  ")); Serial.println(REVERSE_ENGINEER_MODE);
    Serial.print(F("  TC358743 I2C      SDA=")); Serial.print(TC_I2C_SDA_PIN);
    Serial.print(F(" SCL=")); Serial.print(TC_I2C_SCL_PIN);
    Serial.print(F(" ADDR=0x")); Serial.println(TC_I2C_ADDR, HEX);
    Serial.print(F("  TC_RESET_PIN      ")); Serial.println(TC_RESET_PIN);
    Serial.print(F("  OLED_ENABLE       ")); Serial.println(OLED_ENABLE);
    if (OLED_ENABLE) { Serial.print(F("  OLED_I2C_ADDR    0x")); Serial.println(OLED_I2C_ADDR, HEX); }
    Serial.print(F("  RTC_ENABLE        ")); Serial.println(RTC_ENABLE);
    if (RTC_ENABLE) {
        Serial.print(F("  RTC I2C           SDA=")); Serial.print(RTC_I2C_SDA_PIN);
        Serial.print(F(" SCL=")); Serial.print(RTC_I2C_SCL_PIN);
        Serial.print(F(" ADDR=0x")); Serial.println(RTC_I2C_ADDR, HEX);
    }
#if WEBUI_ENABLE
    Serial.print(F("  WEBUI_ENABLE      1    AP=")); Serial.print(WEBUI_AP_SSID);
    if (WEBUI_AP_PASSWORD) { Serial.print(F(" (pass)")); }
    Serial.println();
    if (WEBUI_STA_SSID[0]) {
        Serial.print(F("  STA               ")); Serial.println(WEBUI_STA_SSID);
    } else {
        Serial.println(F("  STA               (none)"));
    }
#endif
    Serial.println(F("────────────────────────────────────────"));
    Serial.println();

    // Start WiFi AP + web server BEFORE the LTC timer — the hardware-timer ISR
    // fires at several kHz and can interfere with WiFi RF calibration if it's
    // already running during softAP().  Give the WiFi stack a clean window.
#if WEBUI_ENABLE
    Serial.print(F("Starting WiFi AP... "));
    webui.begin(WEBUI_AP_SSID, WEBUI_AP_PASSWORD,
                WEBUI_STA_SSID, WEBUI_STA_PASSWORD);
    webui.onSetFps([](uint8_t fps, bool df) {
        ltc.setFps(fps, df);
        framePollMs = 1000 / fps;
        {
            Preferences prefs;
            prefs.begin("ltc", false);
            prefs.putUChar("fps", fps);
            prefs.putBool("df", df);
            prefs.end();
        }
#if FPS_AUTO_DETECT
        if (webui.autoFps()) resetFpsDetect();
#endif
    });
    webui.onJamTime([](uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
        ltc.setTime(hh, mm, ss, ff);
        ltc.setDd(dd);
        Serial.printf("jam %02u:%02u:%02u:%02u:%02u\n", dd, hh, mm, ss, ff);
    });
#if MAX7219_ENABLE
    webui.onSetBrightness([](uint8_t val) {
        mx7219.setIntensity(val);
    });
    webui.onSetMatrixEnabled([](bool en) {
        if (!en) mx7219.clear();
    });
#endif

    // WiFi event logging for AP debugging
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
                Serial.print(F("WiFi: STA connected, MAC="));
                Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                    info.wifi_ap_staconnected.mac[0],
                    info.wifi_ap_staconnected.mac[1],
                    info.wifi_ap_staconnected.mac[2],
                    info.wifi_ap_staconnected.mac[3],
                    info.wifi_ap_staconnected.mac[4],
                    info.wifi_ap_staconnected.mac[5]);
                break;
            case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
                Serial.printf("WiFi: STA disconnected, MAC=%02X:%02X:%02X:%02X:%02X:%02X aid=%d\n",
                    info.wifi_ap_stadisconnected.mac[0],
                    info.wifi_ap_stadisconnected.mac[1],
                    info.wifi_ap_stadisconnected.mac[2],
                    info.wifi_ap_stadisconnected.mac[3],
                    info.wifi_ap_stadisconnected.mac[4],
                    info.wifi_ap_stadisconnected.mac[5],
                    info.wifi_ap_stadisconnected.aid);
                break;
            case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
                Serial.println(F("WiFi: STA got IP"));
                break;
        }
    });
#endif

    // Load persisted FPS/DF from NVS (set by web UI in a previous session)
    {
        Preferences prefs;
        prefs.begin("ltc", true);
        uint8_t savedFps = prefs.getUChar("fps", LTC_FPS);
        bool savedDf = prefs.getBool("df", LTC_DROP_FRAME);
        prefs.end();
        if (savedFps != LTC_FPS || savedDf != LTC_DROP_FRAME) {
            ltc.setFps(savedFps, savedDf);
            framePollMs = 1000 / savedFps;
        }
    }

    // Start LTC encoder AFTER WiFi — the hardware timer ISR is high-frequency
    // and should not be active during WiFi/softAP initialization.
    Serial.println(F("Settling..."));
    delay(500);
    // DEBUG: skip LTC timer to isolate WiFi interference
    #ifndef SKIP_LTC_TIMER
    Serial.print(F("Starting LTC timer... "));
    ltc.begin();
    ltc.setTime(1, 0, 0, 0);
    Serial.println(F("OK"));
    #else
    Serial.println(F("LTC timer SKIPPED (debug)"));
    #endif

    // ---- I2C device detection (non-critical — done after WiFi) ----
    // NOTE: the I2C peripheral is only initialised when the bit-bang probe
    // inside tc.begin() finds a device.  When no TC358743 is present the
    // SDA/SCL pins stay as INPUT_PULLUP (silent) to avoid any noise coupling
    // into the WiFi radio — all downstream I2C probes (RTC, OLED) are skipped.
    tcPresent = tc.begin(TC_I2C_SDA_PIN, TC_I2C_SCL_PIN, TC_RESET_PIN);
    if (!tcPresent) {
        Serial.println(F("ERROR: TC358743 not responding — HDMI functions disabled."));
    } else {
        Serial.println(F("TC358743 detected OK."));
    }

    if (tcPresent) {
#if RTC_ENABLE
        rtcPresent = rtc.begin(RTC_I2C_SDA_PIN, RTC_I2C_SCL_PIN);
        if (rtcPresent) {
            Serial.println(F("DS3231 RTC detected."));
            if (rtc.readTime(rtcHH, rtcMM, rtcSS)) {
                ltc.setTime(rtcHH, rtcMM, rtcSS, 0);
                lastRtcReadMs = lastRtcSyncMs = millis();
                Serial.print(F("RTC initial time: "));
                if (rtcHH < 10) Serial.print('0');
                Serial.print(rtcHH); Serial.print(':');
                if (rtcMM < 10) Serial.print('0');
                Serial.print(rtcMM); Serial.print(':');
                if (rtcSS < 10) Serial.print('0');
                Serial.println(rtcSS);
            }
        } else {
            Serial.println(F("No RTC detected."));
            ltc.setTime(1, 0, 0, 0);
        }
#else
        ltc.setTime(1, 0, 0, 0);
#endif
#if OLED_ENABLE
        if (oled.begin()) {
            Serial.println(F("OLED display initialized."));
        } else {
            Serial.println(F("OLED not detected — skipping."));
        }
#endif
    } else {
#if RTC_ENABLE
        rtcPresent = false;
#endif
        ltc.setTime(1, 0, 0, 0);
        Serial.println(F("Skipping I2C devices (no TC358743)."));
    }

#if FPS_AUTO_DETECT
    Serial.println(F("FPS auto-detect enabled."));
#endif

#if MAX7219_ENABLE
    Serial.print(F("Starting MAX7219 display... "));
    mx7219.begin();
    mx7219.showText("VID-PRO");
    delay(3000);
    Serial.print(MAX7219_NUM_MODULES);
    Serial.println(F(" modules initialized."));
#endif

    Serial.print(F("Starting BLE master... "));
    bleTimecodeInit();
    Serial.println(F("OK"));

    Serial.println(F("LTC generator running. Connect HDMI now."));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
#if WEBUI_ENABLE
    webui.handleClient();
#endif

    // AP health check — print channel + station count every 5 s (only when AP is active)
    static unsigned long lastApDiag = 0;
    unsigned long now = millis();
    if (now - lastApDiag >= 5000) {
        lastApDiag = now;
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
            uint8_t ch;
            wifi_second_chan_t sec;
            esp_wifi_get_channel(&ch, &sec);
            Serial.print(F("AP IP="));
            Serial.print(WiFi.softAPIP());
            Serial.print(F(" ch="));
            Serial.print(ch);
            Serial.print(F(" stations="));
            Serial.println(WiFi.softAPgetStationNum());
        }
    }

    // -----------------------------------------------------------------------
    // REVERSE ENGINEER MODE
    // -----------------------------------------------------------------------
#if REVERSE_ENGINEER_MODE
    if (tcPresent) {
        runReverseEngineerDump();
        delay(500);
        return;
    } else {
        static bool tcAbsentWarned = false;
        if (!tcAbsentWarned) {
            tcAbsentWarned = true;
            Serial.println(F("TC358743 absent — web UI still available."));
        }
    }
#endif

    // -----------------------------------------------------------------------
    // RUN MODE
    // -----------------------------------------------------------------------
    bool locked = tcPresent ? tc.hasSignal() : false;

    // ----- Fast poll: detect new frames via SYS_INT -----
    if (now - lastFastPoll >= FAST_POLL_MS) {
        lastFastPoll = now;

        if (locked) {
            uint16_t sysInt = tc.readReg16(SYS_INT);
            if (sysInt) {
                tc.writeReg16(SYS_INT, 0xFFFF);

                if (sysInt & 0x8000) {
                    uint32_t nowUs = micros();

#if FPS_AUTO_DETECT
                    if (!fpsDetected) {
                        if (lastFrameUs != 0) {
                            uint32_t interval = nowUs - lastFrameUs;
                            if (interval > 1000 && interval < 100000 && intervalCount < 30) {
                                intervals[intervalCount++] = interval;
                            }
                        }
                        lastFrameUs = nowUs;
                    }
#endif

                    Gh5Timecode gh5tc;
                    if (decodeGh5Timecode(tc, gh5tc)) {
                        ltc.setTime(gh5tc.hh, gh5tc.mm, gh5tc.ss, gh5tc.ff);
#if RTC_ENABLE
                        if (rtcPresent && gh5tc.ss != rtcSS) {
                            rtc.setTime(gh5tc.hh, gh5tc.mm, gh5tc.ss);
                            rtcHH = gh5tc.hh; rtcMM = gh5tc.mm; rtcSS = gh5tc.ss;
                            lastRtcSyncMs = now;
                        }
#endif
                    }
                }
            }
        }
    }

    // ----- Frame-synced processing -----
    if (now - lastFramePoll >= framePollMs) {
        lastFramePoll += framePollMs;
        if (now - lastFramePoll >= framePollMs) lastFramePoll = now;

#if FPS_AUTO_DETECT
        if (webui.autoFps() && !fpsDetected && tryDetectFps()) {
        }
#endif

        bool hdmiOk = locked && tcPresent && tc.isHdmiMode();
        digitalWrite(STATUS_LED_PIN, hdmiOk ? HIGH : LOW);

        uint16_t frames = ltc.framesCompleted();
        bool frameDone = hdmiOk || (frames > 0);
        if (!hdmiOk && frameDone) {
#if RTC_ENABLE
            if (rtcPresent) {
                if (now - lastRtcReadMs >= 1000) {
                    lastRtcReadMs = now;
                    if (rtc.readTime(rtcHH, rtcMM, rtcSS)) {
                        lastRtcSyncMs = now;
                    }
                }
                unsigned long elapsed = now - lastRtcSyncMs;
                uint8_t ff = (elapsed * ltc.fps()) / 1000;
                if (ff >= ltc.fps()) ff = ltc.fps() - 1;
                ltc.setTime(rtcHH, rtcMM, rtcSS, ff);
                strcpy(tcSource, "RTC");
            } else {
                while (frames--) ltc.tick();
                strcpy(tcSource, "FREE");
            }
#else
            while (frames--) ltc.tick();
            strcpy(tcSource, "FREE");
#endif
        }
        if (hdmiOk) strcpy(tcSource, "HDMI");

#if FPS_AUTO_DETECT
        if (!locked) resetFpsDetect();
#endif

#if OLED_ENABLE
        fmtTcStr(ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
        oled.update(tcStr, ltc.fps(), hdmiOk);
#endif
#if MAX7219_ENABLE
        if (webui.matrixEnabled()) {
            mx7219.showTimecode(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
        }
#endif
        bleTimecodeUpdate(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());

#if WEBUI_ENABLE
        fmtTcStr(ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
        webui.update(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff(),
                     ltc.fps(), ltc.dropFrame(), hdmiOk, tcSource);
#endif
    }
}
