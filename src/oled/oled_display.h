#pragma once
#include <Arduino.h>
#include "config.h"

#if OLED_ENABLE
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
    void update(const char *timecode, uint8_t fps, uint8_t lockState,
                const char *deviceName, bool autoFps, const char *ltcMode,
                uint8_t slaveCount = 0, uint8_t batteryPct = 255,
                uint8_t masterIndicator = 0,
                bool bleConnected = false);
    void setEnabled(bool en) { _enabled = en; }
    bool enabled() const { return _enabled; }
    Adafruit_SSD1306& display() { return _display; }
    void forceRedraw() { _lastTc[0] = '\0'; }

private:
    Adafruit_SSD1306 _display;
    bool _present;
    bool _enabled = true;
    char _lastTc[13];
    uint8_t _lastFps;
};
#endif
