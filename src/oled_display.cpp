#include "oled_display.h"

OledDisplay::OledDisplay()
    : _u8g2(U8G2_R0, U8X8_PIN_NONE, TC_I2C_SCL_PIN, TC_I2C_SDA_PIN), _present(false), _lastFps(0) {
    _lastTc[0] = '\0';
}

bool OledDisplay::begin() {
    // Probe I2C first — U8g2's init sends ~30 commands, and on ESP32-C3
    // the I2C timeout is set to I2C_LL_MAX_TIMEOUT (≈10 s per NACKed
    // transaction).  Without this probe, a missing/powered-off OLED would
    // block setup() for minutes and starve the WiFi stack.
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

    w = _u8g2.getStrWidth("(c) by VID-PRO");
    _u8g2.drawStr((128 - w) / 2, 63, "(c) by VID-PRO");

    _u8g2.sendBuffer();
    _present = true;
    return true;
}

void OledDisplay::update(const char *timecode, uint8_t fps, bool locked) {
    if (!_present || !_enabled) return;
    if (strcmp(timecode, _lastTc) == 0 && fps == _lastFps) return;

    strncpy(_lastTc, timecode, sizeof(_lastTc) - 1);
    _lastTc[sizeof(_lastTc) - 1] = '\0';
    _lastFps = fps;

    _u8g2.firstPage();
    do {
        _u8g2.setFont(u8g2_font_6x10_tf);
        int w = _u8g2.getStrWidth("HDMI2LTC");
        _u8g2.drawStr((128 - w) / 2, 9, "HDMI2LTC");

        if (locked) {
            _u8g2.setFont(u8g2_font_5x7_tf);
            char fpsStr[8];
            snprintf(fpsStr, sizeof(fpsStr), "%dfps", fps);
            _u8g2.drawStr(126 - _u8g2.getStrWidth(fpsStr), 7, fpsStr);
        }

        _u8g2.setFont(u8g2_font_logisoso24_tf);
        w = _u8g2.getStrWidth(timecode);
        _u8g2.drawStr((128 - w) / 2, 46, timecode);

        _u8g2.setFont(u8g2_font_6x10_tf);
        w = _u8g2.getStrWidth("(c) by VID-PRO");
        _u8g2.drawStr((128 - w) / 2, 63, "(c) by VID-PRO");
    } while (_u8g2.nextPage());
}
