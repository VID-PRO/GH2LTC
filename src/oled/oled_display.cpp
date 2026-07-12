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
#if BLE_MASTER
        "Master"
#elif BLE_SLAVE
        "Slave"
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

void OledDisplay::update(const char *timecode, uint8_t fps, bool locked,
                         const char *role, const char *source,
                         uint8_t slaveCount) {
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

        _u8g2.setFont(u8g2_font_logisoso18_tf);
        const int timecodeX = (128 - _u8g2.getStrWidth("88:88:88:88")) / 2;
        _u8g2.drawStr(timecodeX, 46, timecode);

        _u8g2.setFont(u8g2_font_6x10_tf);
        char bottom[24];
        snprintf(bottom, sizeof(bottom), "%s  %s  %d", role, source, fps);
        w = _u8g2.getStrWidth(bottom);
        _u8g2.drawStr((128 - w) / 2, 63, bottom);
        if (slaveCount > 0) {
            char cnt[6];
            snprintf(cnt, sizeof(cnt), "+%d", slaveCount);
            w = _u8g2.getStrWidth(cnt);
            _u8g2.drawStr(126 - w, 63, cnt);
        }
    } while (_u8g2.nextPage());
}
#endif
