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

private:
    U8G2_SSD1306_128X64_NONAME_1_HW_I2C _u8g2;
    bool _present;
    char _lastTc[13];
    uint8_t _lastFps;
};
