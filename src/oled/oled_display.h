#pragma once
#include <Arduino.h>
#include "config.h"

#if OLED_ENABLE
#include <Wire.h>
#include <U8g2lib.h>

// Allow per-board override of OLED I2C pins (e.g. HDMI has separate OLED bus)
#ifndef OLED_I2C_SDA_PIN
#define OLED_I2C_SDA_PIN  TC_I2C_SDA_PIN
#endif
#ifndef OLED_I2C_SCL_PIN
#define OLED_I2C_SCL_PIN  TC_I2C_SCL_PIN
#endif

class OledDisplay {
public:
    OledDisplay();

    bool begin();
    void update(const char *timecode, uint8_t fps, bool locked,
                const char *deviceName, bool autoFps, const char *ltcMode,
                uint8_t slaveCount = 0, uint8_t batteryPct = 255,
                uint8_t masterIndicator = 0);
    void setEnabled(bool en) { _enabled = en; }
    bool enabled() const { return _enabled; }
    U8G2_SSD1306_128X64_NONAME_1_HW_I2C& u8g2() { return _u8g2; }
    void forceRedraw() { _lastTc[0] = '\0'; }

private:
    U8G2_SSD1306_128X64_NONAME_1_HW_I2C _u8g2;
    bool _present;
    bool _enabled = true;
    char _lastTc[13];
    uint8_t _lastFps;
};
#endif
