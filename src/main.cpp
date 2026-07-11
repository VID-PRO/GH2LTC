#include <Arduino.h>
#include <esp_wifi.h>
#include "esp_system.h"
#include <Preferences.h>
#include <esp_efuse.h>
#include <esp_mac.h>
#include "config.h"
#include "hdmi/tc358743.h"
#include "hdmi/tc358743_regs.h"
#include "ltc/ltc_encoder.h"
#include "hdmi/panasonic_tc.h"
#if RTC_ENABLE
#include "rtc/ds3231.h"
#endif
#if OLED_ENABLE
#include "oled/oled_display.h"
#endif
#if MAX7219_ENABLE
#include "matrix/max7219_display.h"
#endif
#if WEBUI_ENABLE
#include "webui/webui.h"
#endif
#include "timecode/ble_timecode.h"

// ---------------------------------------------------------------------------
// Master-specific globals
// ---------------------------------------------------------------------------
#if BLE_MASTER
TC358743 tc(Wire, TC_I2C_ADDR);
#endif
LtcEncoder ltc(LTC_OUT_PIN, LTC_FPS, LTC_DROP_FRAME);
static char tcStr[13] = "00:00:00:00";

static void fmtTcStr(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
    snprintf(tcStr, sizeof(tcStr), "%02u:%02u:%02u:%02u", hh, mm, ss, ff);
}

#if OLED_ENABLE
static OledDisplay oled;
#endif

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
static Max7219Display mx7219(MAX7219_CS_PIN, MAX7219_NUM_MODULES);
#endif

static bool tcPresent = false;
static char tcSource[8] = "FREE";

// Master fast-poll constants
static const unsigned long FAST_POLL_MS = 8;
unsigned long lastFastPoll = 0;
unsigned long lastFramePoll = 0;
unsigned long framePollMs = 1000 / LTC_FPS;

// ---------------------------------------------------------------------------
// Frame rate auto-detection (master only)
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
// Reverse-engineer mode helpers (master only)
// ---------------------------------------------------------------------------
#if defined(BLE_MASTER) && REVERSE_ENGINEER_MODE
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

void testEdidWrite() {
    const uint16_t TEST_REG = 0x8C00;
    Serial.println(F("--- EDID RAM test (read-only) ---"));
    Serial.print(F("EDID header: "));
    for (int i = 0; i < 8; i++) {
        Serial.print(tc.readReg8(TEST_REG + i), HEX); Serial.print(' ');
    }
    Serial.println();
}

void dumpEdid() {
    static const size_t EDID_SIZE = 256;
    uint8_t buf[EDID_SIZE];
    Serial.println(F("--- EDID readback (byte-by-byte) ---"));
    for (size_t i = 0; i < EDID_SIZE; i++) {
        buf[i] = tc.readReg8(EDID_RAM + i);
    }
    for (size_t i = 0; i < EDID_SIZE; i += 16) {
        char line[80];
        int pos = snprintf(line, sizeof(line), "%04X: ", (unsigned)i);
        for (size_t j = 0; j < 16; j++) {
            pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", buf[i + j]);
        }
        Serial.println(line);
    }
    uint8_t sum0 = 0;
    for (int i = 0; i < 128; i++) sum0 += buf[i];
    Serial.print(F("Block 0 checksum: 0x")); Serial.print(sum0, HEX);
    Serial.println(sum0 == 0 ? F(" (OK)") : F(" (BAD!)"));
    uint8_t sum1 = 0;
    for (int i = 128; i < 256; i++) sum1 += buf[i];
    Serial.print(F("Block 1 checksum: 0x")); Serial.print(sum1, HEX);
    Serial.println(sum1 == 0 ? F(" (OK)") : F(" (BAD!)"));
    Serial.println();
}

void runReverseEngineerDump() {
    uint8_t st = tc.readReg8(SYS_STATUS);
    uint8_t hpd = tc.readReg8(HPD_CTL);
    uint8_t ddc = tc.readReg8(DDC_CTL);

    // If TMDS stayed 0 for ~120s, re-write EDID + retry HPD
    static unsigned long lastHpdRetry = 0;
    unsigned long now = millis();
    if (!(st & 0x02) && (now - lastHpdRetry > 120000)) {
        lastHpdRetry = now;
        Serial.println(F("TMDS still 0 — retrying (full re-apply + CLKM_CTL)..."));
        tc.writeReg8(HPD_CTL, 0x00);
        tc.writeReg8(EDID_MODE, 0x00);
        delay(50);
        tc.pulseReg16(SYSCTL, MASK_HDMIRST);
        delay(200);
        // Re-apply ref clock (42 MHz) + PHY config + EDID
        tc.writeReg8(PHY_CTL0, 0x23);
        tc.writeReg8(CLKM_CTL, 0x01);
        tc.writeReg8(SYS_FREQ0, (42000000 / 10000) & 0xFF);
        tc.writeReg8(SYS_FREQ1, ((42000000 / 10000) >> 8) & 0xFF);
        tc.writeReg8(NCO_F0_MOD, 0x00);
        tc.writeReg8(DDC_CTL, 0x02);
        tc.writeReg8(PHY_EN, 0x00);
        tc.writeReg8(PHY_CTL1, 0x80);
        tc.writeReg8(SYS_CLK, 0x00);
        tc.writeReg8(PHY_BIAS, 0x50);
        tc.writeReg8(PHY_CSQ, 0x02);
        tc.writeReg8(PHY_RST, 0x01);
        tc.writeReg8(AVM_CTL, 45);
        tc.writeReg8(HDMI_DET, 0x10);
        tc.writeReg8(HV_RST, 0x00);
        tc.writeReg8(PHY_EN, 0x01);
        tc.writeReg8(ANA_CTL, 0x33);
        tc.writeReg8(VI_MODE, 0x00);
        tc.writeReg8(VOUT_SET2, 0x01);
        tc.writeReg8(VOUT_SET3, 0x08);
        // Re-write EDID + reshow
        tc.writeEdidByteByByte(EDID_1080P25, sizeof(EDID_1080P25));
        delay(10);
        tc.writeReg8(EDID_LEN1, 0x00);
        tc.writeReg8(EDID_LEN2, 0x00);
        tc.writeReg8(EDID_SEG_NUM, 0x00);
        tc.writeReg8(EDID_LEN1, sizeof(EDID_1080P25) & 0xFF);
        tc.writeReg8(EDID_LEN2, (sizeof(EDID_1080P25) >> 8) & 0xFF);
        tc.writeReg8(EDID_MODE, MASK_EDID_MODE_E_DDC);
        tc.writeReg8(INIT_END, 0x01);
        delay(5);
        delay(200);
        tc.writeReg8(HPD_CTL, 0x01);
        tc.writeReg16(SYS_INT, 0xFFFF);  // clear pending events
        // Poll SYS_STATUS + DDC address rapidly for 2 seconds
        for (int i = 0; i < 40; i++) {
            delay(50);
            uint8_t s = tc.readReg8(SYS_STATUS);
            uint16_t si = tc.readReg16(SYS_INT);
            uint8_t seg = tc.readReg8(EDID_SEG_NUM);
            if (si) {
                Serial.print(F("SYS_INT=0x"));
                Serial.print(si, HEX);
                Serial.print(F(" at +"));
                Serial.print((i + 1) * 50);
                Serial.println(F("ms"));
                tc.writeReg16(SYS_INT, 0xFFFF);
            }
            if (s & 0x02) {
                Serial.print(F("TMDS DETECTED at +"));
                Serial.print((i + 1) * 50);
                Serial.println(F("ms after HPD"));
                break;
            }
            if (seg != 0xA0 && seg != 0) {
                Serial.print(F("DDC addr 0x"));
                Serial.print(seg, HEX);
                Serial.print(F(" at +"));
                Serial.print((i + 1) * 50);
                Serial.println(F("ms"));
            }
        }
        st = tc.readReg8(SYS_STATUS);
        Serial.print(F("After retry SYS_STATUS: 0x"));
        Serial.println(st, HEX);
    }

    Serial.println(F("--- status / packet dump ---"));
    Serial.print(F("SYS_STATUS: 0x")); Serial.print(st, HEX);
    Serial.print(F(" DDC5V=")); Serial.print((st >> 0) & 1);
    Serial.print(F(" TMDS="));  Serial.print((st >> 1) & 1);
    Serial.print(F(" PLL="));   Serial.print((st >> 2) & 1);
    Serial.print(F(" SCDT="));  Serial.print((st >> 3) & 1);
    Serial.print(F(" HDMI="));  Serial.print((st >> 4) & 1);
    Serial.print(F(" HDCP="));  Serial.print((st >> 5) & 1);
    Serial.print(F(" MUTE="));  Serial.print((st >> 6) & 1);
    Serial.print(F(" SYNC="));  Serial.println((st >> 7) & 1);
    // 0x85xx register write-read-back test
    uint8_t testVal = tc.readReg8(SYS_CLK);
    tc.writeReg8(SYS_CLK, 0xA5);
    uint8_t testRead = tc.readReg8(SYS_CLK);
    tc.writeReg8(SYS_CLK, testVal);  // restore
    if (testRead != 0xA5) {
        Serial.print(F("WARN: 0x85xx write/read mismatch! SYS_CLK: wrote 0xA5 got 0x"));
        Serial.println(testRead, HEX);
    }
    uint8_t vi1 = tc.readReg8(VI_STATUS1);
    uint8_t vi3 = tc.readReg8(VI_STATUS3);
    uint8_t phyCsq = tc.readReg8(PHY_CSQ);
    uint16_t sysInt = tc.readReg16(SYS_INT);
    uint8_t edidSeg = tc.readReg8(EDID_SEG);
    uint8_t edidLen1 = tc.readReg8(EDID_LEN1);
    uint8_t edidLen2 = tc.readReg8(EDID_LEN2);
    uint8_t sf0 = tc.readReg8(SYS_FREQ0);
    uint8_t sf1 = tc.readReg8(SYS_FREQ1);
    uint8_t nco = tc.readReg8(NCO_F0_MOD);
    uint8_t fhMin0 = tc.readReg8(FH_MIN0);
    uint8_t fhMin1 = tc.readReg8(FH_MIN1);
    uint8_t fhMax0 = tc.readReg8(FH_MAX0);
    uint8_t fhMax1 = tc.readReg8(FH_MAX1);
    uint8_t ldet0 = tc.readReg8(LOCKDET_REF0);
    uint8_t ldet1 = tc.readReg8(LOCKDET_REF1);
    uint8_t ldet2 = tc.readReg8(LOCKDET_REF2);
    uint8_t phyCtl0 = tc.readReg8(PHY_CTL0);
    uint16_t sysctl = tc.readReg16(SYSCTL);
    Serial.print(F("HPD_CTL: 0x")); Serial.print(hpd, HEX);
    Serial.print(F(" DDC_CTL: 0x")); Serial.print(ddc, HEX);
    Serial.print(F(" PHY_CSQ: 0x")); Serial.print(phyCsq, HEX);
    Serial.print(F(" VI_STAT1: 0x")); Serial.print(vi1, HEX);
    Serial.print(F(" VI_STAT3: 0x")); Serial.print(vi3, HEX);
    Serial.print(F(" SYS_INT: 0x")); Serial.print(sysInt, HEX);
    Serial.print(F(" SF:0x")); Serial.print(sf1, HEX); Serial.print(sf0, HEX);
    Serial.print(F(" FH:")); Serial.print(fhMin1, HEX); Serial.print(fhMin0, HEX);
    Serial.print(F("-")); Serial.print(fhMax1, HEX); Serial.print(fhMax0, HEX);
    Serial.print(F(" LD:")); Serial.print(ldet2, HEX); Serial.print(ldet1, HEX); Serial.print(ldet0, HEX);
    Serial.print(F(" NCO:0x")); Serial.print(nco, HEX);
    Serial.print(F(" PHYCTL0:0x")); Serial.print(phyCtl0, HEX);
    Serial.print(F(" SYSCTL:0x")); Serial.print(sysctl, HEX);
    uint8_t edidSegNum = tc.readReg8(EDID_SEG_NUM);
    Serial.print(F(" EDID_SEG: 0x")); Serial.print(edidSeg, HEX);
    Serial.print(F(" EDID_SEG_NUM: 0x")); Serial.print(edidSegNum, HEX);
    Serial.print(F(" EDID_LEN: 0x")); Serial.print(edidLen2, HEX); Serial.print(edidLen1, HEX);
    Serial.println();
    // Verify EDID_RAM was written — read first 16 bytes (should be EDID header)
    uint8_t edidHdr[16];
    for (int i = 0; i < 16; i++) edidHdr[i] = tc.readReg8(EDID_RAM + i);
    Serial.print(F("EDID_RAM[0..15]: "));
    for (int i = 0; i < 16; i++) { if (edidHdr[i] < 0x10) Serial.print('0'); Serial.print(edidHdr[i], HEX); Serial.print(' '); }
    Serial.println();
    Serial.print(F("Signal locked: "));
    Serial.println(tc.hasSignal() ? F("yes") : F("no"));
    Serial.print(F("HDMI mode: "));
    Serial.println(tc.isHdmiMode() ? F("yes (good, packets active)") : F("no (DVI - no packets!)"));

    dumpEdid();
    // Verify CEA block was written — dump first 20 bytes of block 1
    Serial.print(F("CEA_BLK1[0..19]: "));
    for (int i = 0; i < 20; i++) {
        uint8_t b = tc.readReg8(EDID_RAM + 128 + i);
        if (b < 0x10) Serial.print('0');
        Serial.print(b, HEX); Serial.print(' ');
    }
    Serial.println();

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
// Slave-specific: BLE timecode callback
// ---------------------------------------------------------------------------
static void onBleTimecode(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
    ltc.setTime(hh, mm, ss, ff);
    ltc.setDd(dd);
}

// ---------------------------------------------------------------------------
// Common: print reset reason
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

// ---------------------------------------------------------------------------
// Common: config dump
// ---------------------------------------------------------------------------
static void printConfig() {
    Serial.println(F("── CONFIG ──────────────────────────────"));
    Serial.print(F("  MODE              "));
#if BLE_CLAP
    Serial.println(F("CLAP"));
#elif BLE_MASTER
    Serial.println(F("MASTER"));
#else
    Serial.println(F("SLAVE"));
#endif
#ifndef BLE_CLAP
    Serial.print(F("  LTC_ENABLED       ")); Serial.println(webui.ltcEnabled() ? "1" : "0");
    Serial.print(F("  LTC_OUT_PIN       ")); Serial.println(LTC_OUT_PIN);
    Serial.print(F("  LTC_FPS           ")); Serial.println(LTC_FPS);
    Serial.print(F("  LTC_DROP_FRAME    ")); Serial.println(LTC_DROP_FRAME);
#endif
    Serial.print(F("  FPS_AUTO_DETECT   ")); Serial.println(FPS_AUTO_DETECT);
#ifdef REVERSE_ENGINEER_MODE
    Serial.print(F("  REVERSE_ENGINEER  ")); Serial.println(REVERSE_ENGINEER_MODE);
#endif
#if BLE_MASTER
    Serial.print(F("  TC358743 I2C      SDA=")); Serial.print(TC_I2C_SDA_PIN);
    Serial.print(F(" SCL=")); Serial.print(TC_I2C_SCL_PIN);
    Serial.print(F(" ADDR=0x")); Serial.println(TC_I2C_ADDR, HEX);
    Serial.print(F("  TC_RESET_PIN      ")); Serial.println(TC_RESET_PIN);
#endif
#ifndef BLE_CLAP
    Serial.print(F("  OLED_ENABLED      ")); Serial.println(webui.oledEnabled() ? "1" : "0");
    if (OLED_ENABLE) { Serial.print(F("  OLED_I2C_ADDR    0x")); Serial.println(OLED_I2C_ADDR, HEX); }
#endif
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
}

// ===========================================================================
// Master-specific: I2C/HDMI setup
// ===========================================================================
#if BLE_MASTER
static void masterSetup() {
    // Load persisted FPS/DF from NVS
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

    // Start LTC encoder
#ifndef SKIP_LTC_TIMER
    ltc.begin();
    ltc.setTime(1, 0, 0, 0);
    Serial.print(F("LTC encoder "));
    Serial.println(webui.ltcEnabled() ? F("started") : F("disabled"));
#else
    Serial.println(F("LTC timer SKIPPED (debug)"));
#endif

    // TC358743 probe via hardware I2C — try default address, then alternatives
    Serial.printf("I2C: SDA=%d SCL=%d RST=%d\n",
                  TC_I2C_SDA_PIN, TC_I2C_SCL_PIN, TC_RESET_PIN);
    tcPresent = tc.begin(TC_I2C_SDA_PIN, TC_I2C_SCL_PIN, TC_RESET_PIN);
    if (!tcPresent) {
        // tc.begin() called Wire.end() on failure; reinit for further probes
        Wire.begin(TC_I2C_SDA_PIN, TC_I2C_SCL_PIN, 100000);
        static const uint8_t altAddrs[] = { 0x0F, 0x1F, 0x3D };
        for (int i = 0; i < 3; i++) {
            if (altAddrs[i] == TC_I2C_ADDR) continue;
            Wire.beginTransmission(altAddrs[i]);
            if (Wire.endTransmission() == 0) {
                Serial.printf("TC358743 found at alt address 0x%02X\n", altAddrs[i]);
                tcPresent = true;
                break;
            }
        }
    }

    // Full I2C bus scan
    Serial.print(F("I2C bus: "));
    int nFound = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (nFound) Serial.print(F(", "));
            Serial.printf("0x%02X", addr);
            nFound++;
            // Tag known devices
            if (addr == 0x0F) Serial.print(F("(TC358743)"));
            else if (addr == 0x1F) Serial.print(F("(TC358743?)"));
            else if (addr == 0x3C) Serial.print(F("(OLED)"));
            else if (addr == 0x3D) Serial.print(F("(TC358743?)"));
            else if (addr == 0x68) Serial.print(F("(DS3231)"));
        }
    }
    if (nFound == 0) Serial.print(F("(none) — I2C bus not working or no devices"));
    Serial.println();
    if (!tcPresent) {
        Serial.println(F("ERROR: TC358743 not responding — HDMI disabled."));
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
        if (webui.oledEnabled()) {
            if (oled.begin()) {
                Serial.println(F("OLED display initialized."));
            } else {
                Serial.println(F("OLED not detected — skipping."));
            }
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
    mx7219.setIntensity(webui.brightness());
    if (webui.matrixEnabled()) {
        mx7219.showText("MASTER");
        delay(3000);
    }
    Serial.print(MAX7219_NUM_MODULES);
    Serial.println(F(" modules initialized."));
#endif
}
#endif // BLE_MASTER

// ===========================================================================
// Master-specific: loop
// ===========================================================================
#if BLE_MASTER
static void masterLoop() {
    unsigned long now = millis();

    // AP health check
    static unsigned long lastApDiag = 0;
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

    bool locked = tcPresent ? tc.hasSignal() : false;

    // Fast poll: detect new frames via SYS_INT
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

    // Frame-synced processing
    if (now - lastFramePoll >= framePollMs) {
        lastFramePoll += framePollMs;
        if (now - lastFramePoll >= framePollMs) lastFramePoll = now;

#if FPS_AUTO_DETECT
        if (webui.autoFps() && !fpsDetected && tryDetectFps()) {
        }
#endif

        bool hdmiOk = locked && tcPresent && tc.isHdmiMode();

        static bool prevHdmiOk = false;
        if (hdmiOk != prevHdmiOk) {
            tc.enableCsiStream(hdmiOk);
            prevHdmiOk = hdmiOk;
        }

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
            while (frames--) {
                ltc.tick();
#if MAX7219_ENABLE
#if WEBUI_ENABLE
                if (webui.matrixEnabled())
#endif
                    mx7219.showTimecode(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
#endif
            }
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
        if (webui.oledEnabled()) {
            fmtTcStr(ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
            oled.update(tcStr, ltc.fps(), hdmiOk, "Master", tcSource, bleTimecodeConnectedCount());
        }
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
#endif // BLE_MASTER

// ===========================================================================
// Slave-specific: setup
// ===========================================================================
#if BLE_SLAVE
static void slaveSetup() {
#if OLED_ENABLE || RTC_ENABLE
    Wire.begin(TC_I2C_SDA_PIN, TC_I2C_SCL_PIN);
#endif

    ltc.begin();
    ltc.setTime(0, 0, 0, 0);
#ifndef BLE_CLAP
    Serial.print(F("LTC encoder "));
    Serial.println(webui.ltcEnabled() ? F("started") : F("disabled"));
#endif

#if RTC_ENABLE
    rtcPresent = rtc.begin(RTC_I2C_SDA_PIN, RTC_I2C_SCL_PIN);
    if (rtcPresent) {
        Serial.println(F("DS3231 RTC detected."));
        if (rtc.readTime(rtcHH, rtcMM, rtcSS)) {
            ltc.setTime(rtcHH, rtcMM, rtcSS, 0);
            lastRtcReadMs = lastRtcSyncMs = millis();
            Serial.print(F("RTC initial time: "));
            Serial.printf("%02u:%02u:%02u\n", rtcHH, rtcMM, rtcSS);
        }
    } else {
        Serial.println(F("No RTC detected."));
    }
#endif

#if OLED_ENABLE
    if (webui.oledEnabled()) {
        oled.begin();
        Serial.println(F("OLED started"));
    } else {
        Serial.println(F("OLED disabled"));
    }
#endif

#if MAX7219_ENABLE
    mx7219.begin();
    mx7219.setIntensity(webui.brightness());
    if (webui.matrixEnabled()) {
#if BLE_CLAP
        mx7219.showText("CLAP");
#else
        mx7219.showText("SLAVE");
#endif
        delay(2000);
    }
    Serial.println(F("MAX7219 started"));
#endif

    Serial.println(F("Ready. Scanning for master..."));
}

// ===========================================================================
// Slave-specific: loop
// ===========================================================================
static void slaveLoop() {
    bleTimecodePoll();

    uint16_t frames = ltc.framesCompleted();
    if (frames > 0) {
#if RTC_ENABLE
        if (!bleTimecodeConnected() && rtcPresent) {
            unsigned long now = millis();
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
        } else
#endif
        {
            while (frames--) ltc.tick();
        }
    }

#if OLED_ENABLE
    if (webui.oledEnabled()) {
        fmtTcStr(ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
        bool bleOk = bleTimecodeConnected();
        oled.update(tcStr, ltc.fps(), bleOk, "Slave", bleOk ? "LINK" : "FREE");
    }
#endif

#if MAX7219_ENABLE
#if WEBUI_ENABLE
    if (webui.matrixEnabled()) {
        mx7219.showTimecode(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
    }
#else
    mx7219.showTimecode(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
#endif
    mx7219.setBleConnected(bleTimecodeConnected());
#endif

#if WEBUI_ENABLE
    webui.update(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff(),
                 ltc.fps(), ltc.dropFrame(), bleTimecodeConnected(),
                 bleTimecodeConnected() ? "BLE" : "SCAN");
#endif
}
#endif // BLE_SLAVE

// ===========================================================================
// setup()
// ===========================================================================
void setup() {
    Serial.begin(115200);
    pinMode(LTC_OUT_PIN, OUTPUT);
    delay(2000);
    printResetReason();

    Serial.print(F("GH5 HDMI -> LTC box starting in "));
#if BLE_CLAP
    Serial.println(F("CLAP mode..."));
#elif BLE_MASTER
    Serial.println(F("MASTER mode..."));
#else
    Serial.println(F("SLAVE mode..."));
#endif
    Serial.println();

    // Start WiFi AP + web server BEFORE LTC timer
#if WEBUI_ENABLE
    // Build AP SSID from BLE name: "GH2LTC_XXXX" (master), "TC-SLAVE-XXXX" (slave),
    // "TC-CLAP-XXXX" (clap), or custom name if saved
    static char apSsid[33];
    {
        Preferences blePrefs;
        blePrefs.begin("ble", false);
        uint8_t mac[6] = {};
        esp_efuse_mac_get_default(mac);
        char macSuffix[8];
        snprintf(macSuffix, sizeof(macSuffix), "%02X%02X", mac[4], mac[5]);

#if BLE_CLAP
        char defaultClapName[33];
        snprintf(defaultClapName, sizeof(defaultClapName), "TC-CLAP-%s", macSuffix);
        String bleName = blePrefs.getString("slave_name", defaultClapName);
        strncpy(apSsid, bleName.c_str(), sizeof(apSsid) - 1);
        apSsid[sizeof(apSsid) - 1] = '\0';
#elif BLE_MASTER
        String bleName = blePrefs.getString("name", "TC-LTC-MASTER");
        if (bleName == "TC-LTC-MASTER") {
            snprintf(apSsid, sizeof(apSsid), "GH2LTC_%s", macSuffix);
        } else {
            strncpy(apSsid, bleName.c_str(), sizeof(apSsid) - 1);
            apSsid[sizeof(apSsid) - 1] = '\0';
        }
#else
        char defaultSlaveName[33];
        snprintf(defaultSlaveName, sizeof(defaultSlaveName), "TC-SLAVE-%s", macSuffix);
        String bleName = blePrefs.getString("slave_name", defaultSlaveName);
        strncpy(apSsid, bleName.c_str(), sizeof(apSsid) - 1);
        apSsid[sizeof(apSsid) - 1] = '\0';
#endif
        blePrefs.end();
    }
    // Register persistent callbacks BEFORE begin() so NVS state applies at once
#if OLED_ENABLE
    webui.onSetOledEnabled([](bool en) {
        oled.setEnabled(en);
    });
#endif
    webui.onSetLtcEnabled([](bool en) {
        ltc.setEnabled(en);
    });

    Serial.print(F("Starting WiFi AP... "));
    Serial.println(apSsid);
    webui.begin(apSsid, WEBUI_AP_PASSWORD,
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
        if (en) {
            mx7219.showTimecode(ltc.dd(), ltc.hh(), ltc.mm(), ltc.ss(), ltc.ff());
        } else {
            mx7219.clear();
        }
    });
#endif

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

    printConfig();

    Serial.println();

    // Init BLE (starts server for master, scanner for slave)
    {
        Serial.print(F("BLE init... "));
        bleTimecodeInit();
#if BLE_SLAVE
        bleTimecodeSetCallback(onBleTimecode);
#endif
        Serial.println(F("done"));
    }

#if BLE_MASTER
    masterSetup();
#else
    slaveSetup();
#endif

    Serial.println(F("System ready."));
}

// ===========================================================================
// loop()
// ===========================================================================
void loop() {
#if WEBUI_ENABLE
    webui.handleClient();
#endif

#if BLE_MASTER
    masterLoop();
#else
    slaveLoop();
#endif
}
