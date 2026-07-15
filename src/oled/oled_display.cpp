#include "oled_display.h"

#if OLED_ENABLE
#include <Fonts/FreeMono9pt7b.h>

OledDisplay::OledDisplay()
    : _display(128, 64, &Wire, -1), _present(false), _lastFps(0) {
    _lastTc[0] = '\0';
}

bool OledDisplay::begin() {
    // Wire is already initialized at pins 6/7 from setup().
    // Don't end/reinit — that can leave the NG I2C peripheral in a bad state.
    if (!_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        return false;
    }

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);

    int16_t x1, y1;
    uint16_t w, h;

    _display.getTextBounds("HDMI2LTC", 0, 0, &x1, &y1, &w, &h);
    _display.setCursor((128 - w) / 2, 0);
    _display.print("HDMI2LTC");

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
    _display.getTextBounds(bottom, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor((128 - w) / 2, 54);
    _display.print(bottom);

    _display.display();
    _present = true;
    return true;
}

static void drawWifiIcon(Adafruit_SSD1306 &d, int x, int y) {
    d.fillRect(x + 1, y,     6, 2, SSD1306_WHITE);
    d.fillRect(x + 2, y + 3, 4, 2, SSD1306_WHITE);
    d.fillRect(x + 3, y + 6, 2, 2, SSD1306_WHITE);
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

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);

    // ── Top line: WiFi icon + device name + battery + runtime ──
    int16_t x1, y1;
    uint16_t w, h;

    drawWifiIcon(_display, 0, 0);

    int bx = 95;

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
    _display.getTextBounds(runtime, 0, 0, &x1, &y1, &w, &h);
    rw = w;
    int rx = 128 - rw;

    // battery icon
    _display.drawRect(bx, 2, 12, 7, SSD1306_WHITE);
    _display.fillRect(bx + 12, 4, 2, 3, SSD1306_WHITE);
    if (batteryPct <= 100) {
        uint8_t fill = (batteryPct * 10) / 100;
        if (fill > 10) fill = 10;
        if (fill > 0) _display.fillRect(bx + 1, 3, fill, 5, SSD1306_WHITE);
    }

    // runtime text (right-aligned)
    _display.setCursor(rx, 1);
    _display.print(runtime);

    // device name: centered between WiFi icon and battery, truncated to fit
    const char *name = deviceName ? deviceName : "";
    _display.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);
    int avail = bx - 8;  // pixels available (WiFi end → battery start)
    if (w > avail) {
        // truncate — 6 px/char
        size_t maxChars = (size_t)avail / 6;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*s", (int)maxChars, name);
        _display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
        _display.setCursor(8 + (avail - w) / 2, 1);
        _display.print(buf);
    } else {
        _display.setCursor(8 + (avail - w) / 2, 1);
        _display.print(name);
    }

    // ── Timecode (centered between top line end and bottom boxes) ──
    _display.setFont(&FreeMono9pt7b);
    _display.getTextBounds(timecode, 0, 0, &x1, &y1, &w, &h);
    // FreeMono9pt7b: all glyphs xAdvance=11, so 11 chars = 121px fixed span
    // Fixed x position prevents jumping when character shapes differ
    int tcY = 28 - y1 - h / 2;  // 28 = midpoint of y=10 to y=46
    _display.setCursor(3, tcY);
    _display.print(timecode);

    // ── Bottom line: 4 bordered boxes ──
    _display.setFont(NULL);
    _display.setTextSize(1);
    const int by = 46, bh = 12;

    // Box 1: master indicator (H/L), lock icon (B), or free (F)
    _display.drawRect(0, by, 14, bh, SSD1306_WHITE);
    char mCh;
    if (masterIndicator) {
        mCh = (char)masterIndicator;
    } else if (locked) {
        mCh = 'B';
    } else {
        mCh = 'F';
    }
    char mStr[2] = { mCh, '\0' };
    _display.getTextBounds(mStr, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor((14 - w) / 2, by + 2);
    _display.print(mStr);

    // Box 2: A/M
    _display.drawRect(16, by, 16, bh, SSD1306_WHITE);
    char modeCh[2] = { autoFps ? 'A' : 'M', '\0' };
    _display.getTextBounds(modeCh, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor(16 + (16 - w) / 2, by + 2);
    _display.print(modeCh);

    // Box 3: fps
    _display.drawRect(34, by, 42, bh, SSD1306_WHITE);
    char fpsStr[8];
    snprintf(fpsStr, sizeof(fpsStr), "%dfps", fps);
    _display.getTextBounds(fpsStr, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor(34 + (42 - w) / 2, by + 2);
    _display.print(fpsStr);

    // Box 4: LTC mode
    _display.drawRect(78, by, 50, bh, SSD1306_WHITE);
    char ltcFull[12];
    snprintf(ltcFull, sizeof(ltcFull), "LTC %s", ltcMode);
    _display.getTextBounds(ltcFull, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor(78 + (50 - w) / 2, by + 2);
    _display.print(ltcFull);

    _display.display();
}
#endif
