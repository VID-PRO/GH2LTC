#include "oled_display.h"

#if OLED_ENABLE
OledDisplay::OledDisplay()
    : _u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_I2C_SCL_PIN, OLED_I2C_SDA_PIN), _present(false), _lastFps(0) {
    _lastTc[0] = '\0';
}

bool OledDisplay::begin() {
    Wire.begin(OLED_I2C_SDA_PIN, OLED_I2C_SCL_PIN, 100000);
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        _present = false;
        return false;
    }

    _u8g2.begin();
    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x10_tf);

    int w = _u8g2.getStrWidth("HDMI2LTC");
    _u8g2.drawStr((128 - w) / 2, 9, "HDMI2LTC");

    const char *role =
#if TCWL_HDMI
        "HDMI"
#elif TCWL_LTC
        "LTC"
#else
        "?"
#endif
        ;
    char bottom[24];
    snprintf(bottom, sizeof(bottom), "%s  --  --", role);
    w = _u8g2.getStrWidth(bottom);
    _u8g2.drawStr((128 - w) / 2, 63, bottom);

    _u8g2.sendBuffer();
    _present = true;
    return true;
}

static void drawWifiIcon(U8G2 &u, int x, int y) {
    u.drawBox(x + 1, y, 6, 2);        // outermost arc (top)
    u.drawBox(x + 2, y + 3, 4, 2);    // middle arc
    u.drawBox(x + 3, y + 6, 2, 2);    // innermost arc (bottom)
}

void OledDisplay::update(const char *timecode, uint8_t fps, bool locked,
                         const char *deviceName, bool autoFps,
                         const char *ltcMode, uint8_t slaveCount,
                         uint8_t batteryPct, uint8_t masterIndicator) {
    (void)slaveCount;
    if (!_present || !_enabled) return;
    if (strcmp(timecode, _lastTc) == 0 && fps == _lastFps) return;

    strncpy(_lastTc, timecode, sizeof(_lastTc) - 1);
    _lastTc[sizeof(_lastTc) - 1] = '\0';
    _lastFps = fps;

    _u8g2.firstPage();
    do {
        // ── Top line: WiFi icon + device name + battery + runtime ──
        drawWifiIcon(_u8g2, 0, 0);
        _u8g2.setFont(u8g2_font_6x10_tf);

        int bx = 95;    // battery x (moved left to make room for runtime text on right)

        char runtime[10] = "";
        int rw = 0;
        if (batteryPct <= 100) {
            unsigned remMin = (unsigned)batteryPct * BAT_FULL_RUNTIME_MIN / 100;
            unsigned hrs = remMin / 60;
            if (hrs >= 1)
                snprintf(runtime, sizeof(runtime), "%uh", hrs);
            else
                snprintf(runtime, sizeof(runtime), "%um", remMin);
        } else {
            strcpy(runtime, "--");
        }
        rw = _u8g2.getStrWidth(runtime);
        int rx = 128 - rw;

        // battery icon
        _u8g2.drawFrame(bx, 2, 12, 7);
        _u8g2.drawBox(bx + 12, 4, 2, 3);
        if (batteryPct <= 100) {
            uint8_t fill = (batteryPct * 10) / 100;
            if (fill > 10) fill = 10;
            if (fill > 0) _u8g2.drawBox(bx + 1, 3, fill, 5);
        }

        // runtime text
        _u8g2.setFont(u8g2_font_6x10_tf);
        _u8g2.drawStr(rx, 10, runtime);

        const char *name = deviceName ? deviceName : "";
        _u8g2.setFont(u8g2_font_8x13_tf);
        int nw = _u8g2.getStrWidth(name);
        int nx = 8 + (bx - 8 - nw) / 2;   // centered between wifi icon and battery
        if (nx < 8) nx = 8;
        _u8g2.drawStr(nx, 11, name);

        // ── Timecode ──────────────────────────────────────────
        _u8g2.setFont(u8g2_font_logisoso18_tf);
        const int tcX = (128 - _u8g2.getStrWidth("88:88:88:88")) / 2;
        _u8g2.drawStr(tcX, 42, timecode);

        // ── Bottom line: 4 bordered boxes ────────────────────
        _u8g2.setFont(u8g2_font_6x10_tf);
        const int by = 50, bh = 12;

        // Box 1: master indicator ('H'/'L'), lock icon (synced), or BLE icon (not synced)
        _u8g2.drawFrame(0, by, 14, bh);
        if (masterIndicator) {
            char mStr[2] = { (char)masterIndicator, '\0' };
            int mw = _u8g2.getStrWidth(mStr);
            _u8g2.drawStr((14 - mw) / 2, 60, mStr);
        } else {
            _u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
            _u8g2.drawGlyph(3, 60, locked ? 0x44 : 0x42);
            _u8g2.setFont(u8g2_font_6x10_tf);
        }

        // Box 2: A/M
        _u8g2.drawFrame(16, by, 16, bh);
        char modeCh[2] = { autoFps ? 'A' : 'M', '\0' };
        _u8g2.drawStr(16 + (16 - _u8g2.getStrWidth(modeCh)) / 2, 60, modeCh);

        // Box 3: fps
        _u8g2.drawFrame(34, by, 42, bh);
        char fpsStr[8];
        snprintf(fpsStr, sizeof(fpsStr), "%dfps", fps);
        _u8g2.drawStr(34 + (42 - _u8g2.getStrWidth(fpsStr)) / 2, 60, fpsStr);

        // Box 4: LTC mode
        _u8g2.drawFrame(78, by, 50, bh);
        char ltcFull[12];
        snprintf(ltcFull, sizeof(ltcFull), "LTC %s", ltcMode);
        _u8g2.drawStr(78 + (50 - _u8g2.getStrWidth(ltcFull)) / 2, 60, ltcFull);
    } while (_u8g2.nextPage());
}
#endif
