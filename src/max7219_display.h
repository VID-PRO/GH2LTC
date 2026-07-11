#pragma once
#include <Arduino.h>
#include <MD_MAX72xx.h>

class Max7219Display {
public:
    Max7219Display(uint8_t csPin, uint8_t numModules);

    void begin();
    void setIntensity(uint8_t val);
    void clear();
    void showTimecode(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff);
    void showText(const char *text);
    void setBleConnected(bool en);

private:
    MD_MAX72XX _mx;
    char _lastTc[11];
    bool _bleConnected = false;

    void _drawChar(uint8_t *buf, uint8_t col, char c);
    static const uint8_t _font[25][5];
};
