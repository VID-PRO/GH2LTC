#include "max7219_display.h"

// 5x7 font — bit 0 = top row, bit 6 = bottom row.
// Indices 0-9 = '0'-'9', 10 = ':', 11='V', 12='I', 13='D', 14='-', 15='P', 16='R', 17='O'
const uint8_t Max7219Display::_font[18][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x72, 0x49, 0x49, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x0F, 0x08, 0x08, 0x7F, 0x08}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3A, 0x45, 0x45, 0x45, 0x38}, // 6
    {0x01, 0x01, 0x71, 0x09, 0x07}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x26, 0x49, 0x49, 0x49, 0x3E}, // 9
    {0x00, 0x24, 0x00, 0x24, 0x00}, // :  (dots at rows 2-3 and 5-6)
    {0x07, 0x1C, 0x60, 0x1C, 0x07}, // V
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // I
    {0x7F, 0x41, 0x41, 0x41, 0x3E}, // D
    {0x00, 0x08, 0x08, 0x08, 0x00}, // -
    {0x7F, 0x11, 0x11, 0x11, 0x0E}, // P
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
};

// Constructor: software SPI (moduleType, dataPin, clkPin, csPin, numDevices)
Max7219Display::Max7219Display(uint8_t dinPin, uint8_t csPin, uint8_t clkPin, uint8_t numModules)
    : _mx(MD_MAX72XX::FC16_HW, dinPin, clkPin, csPin, numModules)
{
    _lastTc[0] = '\0';
}

void Max7219Display::begin() {
    _mx.begin();
    _mx.control(MD_MAX72XX::INTENSITY, 4);
    _mx.clear();
    _mx.update();
}

void Max7219Display::setIntensity(uint8_t val) {
    _mx.control(MD_MAX72XX::INTENSITY, constrain(val, 0, 15));
}

void Max7219Display::clear() {
    _mx.clear();
    _mx.update();
    _lastTc[0] = '\0';
}

void Max7219Display::showTimecode(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
    char tc[11];
    snprintf(tc, sizeof(tc), "%02u%02u%02u%02u%02u", dd, hh, mm, ss, ff);

    if (strcmp(tc, _lastTc) == 0) return;
    strcpy(_lastTc, tc);

    // Local framebuf
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    // Format: dd:hh:mm:ss.ff
    // 10 digits × 5 cols + 4 separators × 3 cols = 62 cols. Start col 1.
    uint8_t col = 1;

    for (uint8_t i = 0; i < 10; i++) {
        _drawChar(buf, col, tc[i]);
        col += 5;

        if (i == 1 || i == 3 || i == 5 || i == 7) {
            uint8_t sepVal = (i == 7) ? 0x40 : 0x24;
            buf[col]     = 0x00;
            buf[col + 1] = sepVal;
            buf[col + 2] = 0x00;
            col += 3;
        }
    }

    for (uint8_t i = 0; i < 64; i++) {
        _mx.setColumn(i, buf[63 - i]);
    }
    _mx.update();
}

void Max7219Display::showText(const char *text) {
    uint8_t len = 0;
    while (text[len]) len++;

    uint8_t totalCols = len * 5 + (len - 1);
    uint8_t col = (totalCols >= 64) ? 0 : (64 - totalCols) / 2;

    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    for (uint8_t i = 0; i < len; i++) {
        _drawChar(buf, col, text[i]);
        col += 5;
        if (i < len - 1) {
            col += 1;
        }
    }

    for (uint8_t i = 0; i < 64; i++) {
        _mx.setColumn(i, buf[63 - i]);
    }
    _mx.update();
}

void Max7219Display::_drawChar(uint8_t *buf, uint8_t col, char c) {
    uint8_t idx;
    if (c >= '0' && c <= '9') {
        idx = c - '0';
    } else if (c == ':') {
        idx = 10;
    } else if (c == 'V') {
        idx = 11;
    } else if (c == 'I') {
        idx = 12;
    } else if (c == 'D') {
        idx = 13;
    } else if (c == '-') {
        idx = 14;
    } else if (c == 'P') {
        idx = 15;
    } else if (c == 'R') {
        idx = 16;
    } else if (c == 'O') {
        idx = 17;
    } else {
        return;
    }

    for (uint8_t i = 0; i < 5; i++) {
        buf[col + i] = _font[idx][i];
    }
}