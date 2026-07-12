#include "max7219_display.h"
#include <SPI.h>

// 5x7 font — bit 0 = top row, bit 6 = bottom row.
// Indices 0-9 = '0'-'9', 10 = ':', 11='V', 12='I', 13='D', 14='-', 15='P', 16='R', 17='O',
// 18='M', 19='A', 20='S', 21='T', 22='E', 23='L', 24='C', 25='W'
const uint8_t Max7219Display::_font[26][5] = {
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
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
    {0x7E, 0x09, 0x0A, 0x0C, 0x78}, // A
    {0x26, 0x49, 0x49, 0x49, 0x32}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x7F, 0x49, 0x49, 0x49, 0x49}, // E
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
};

// Constructor: hardware SPI (csPin, numModules)
Max7219Display::Max7219Display(uint8_t csPin, uint8_t numModules)
    : _mx(MD_MAX72XX::FC16_HW, csPin, numModules)
{
    _lastTc[0] = '\0';
}

void Max7219Display::begin() {
    SPI.begin(MAX7219_CLK_PIN, -1, MAX7219_DIN_PIN, -1);
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

    if (_bleConnected) {
        buf[63] |= 0xC0;
        buf[62] |= 0x80;
    }

    _mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::ON);
    _mx.update(MD_MAX72XX::OFF);
    for (uint8_t i = 0; i < 64; i++) {
        _mx.setColumn(i, buf[63 - i]);
    }
    _mx.update();
    _mx.update(MD_MAX72XX::ON);
    _mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
}

void Max7219Display::setBleConnected(bool en) {
    if (_bleConnected == en) return;
    _bleConnected = en;
    _mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::ON);
    _mx.update(MD_MAX72XX::OFF);
    if (en) {
        _mx.setColumn(63, _mx.getColumn(63) | 0xC0);
        _mx.setColumn(62, _mx.getColumn(62) | 0x80);
    } else {
        _mx.setColumn(63, _mx.getColumn(63) & ~0xC0);
        _mx.setColumn(62, _mx.getColumn(62) & ~0x80);
    }
    _mx.update();
    _mx.update(MD_MAX72XX::ON);
    _mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
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

    _mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::ON);
    _mx.update(MD_MAX72XX::OFF);
    for (uint8_t i = 0; i < 64; i++) {
        _mx.setColumn(i, buf[63 - i]);
    }
    _mx.update();
    _mx.update(MD_MAX72XX::ON);
    _mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
}

void Max7219Display::_drawChar(uint8_t *buf, uint8_t col, char c) {
    // Convert lowercase to uppercase
    if (c >= 'a' && c <= 'z') c -= 32;

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
    } else if (c == 'M') {
        idx = 18;
    } else if (c == 'A') {
        idx = 19;
    } else if (c == 'S') {
        idx = 20;
    } else if (c == 'T') {
        idx = 21;
    } else if (c == 'E') {
        idx = 22;
    } else if (c == 'L') {
        idx = 23;
    } else if (c == 'C') {
        idx = 24;
    } else if (c == 'W') {
        idx = 25;
    } else {
        return;
    }

    for (uint8_t i = 0; i < 5; i++) {
        buf[col + i] = _font[idx][i];
    }
}