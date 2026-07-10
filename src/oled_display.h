#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "config.h"

class OledDisplay {
public:
    OledDisplay();

    bool begin();
    void update(const char *timecode, uint8_t fps, bool locked);
    void setEnabled(bool en) { _enabled = en; }
    bool enabled() const { return _enabled; }

private:
    U8G2_SSD1306_128X64_NONAME_1_HW_I2C _u8g2;
    bool _present;
    bool _enabled = true;
    char _lastTc[13];
    uint8_t _lastFps;
};
