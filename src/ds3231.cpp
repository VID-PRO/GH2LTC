#include "ds3231.h"

DS3231::DS3231(TwoWire &wire, uint8_t addr) : _wire(wire), _addr(addr), _present(false) {}

bool DS3231::begin(int sda_pin, int scl_pin) {
    (void)sda_pin;
    (void)scl_pin;
    // Wire is shared with TC358743 / OLED and must be initialised by the
    // caller (tc.begin()) before this is called.  We just probe for presence.
    _present = isPresent();
    return _present;
}

bool DS3231::isPresent() {
    _wire.beginTransmission(_addr);
    return _wire.endTransmission() == 0;
}

bool DS3231::readTime(uint8_t &hh, uint8_t &mm, uint8_t &ss) {
    if (!_present) return false;

    _wire.beginTransmission(_addr);
    _wire.write(0x00);
    if (_wire.endTransmission(false) != 0) return false;

    if (_wire.requestFrom((int)_addr, 3) != 3) return false;

    uint8_t secReg = _wire.read();
    uint8_t minReg = _wire.read();
    uint8_t hrReg  = _wire.read();

    ss = bcdToBin(secReg & 0x7F);
    mm = bcdToBin(minReg & 0x7F);

    if (hrReg & 0x40) {
        hh = bcdToBin(hrReg & 0x1F);
        if (hrReg & 0x20) hh += 12;
        hh %= 24;
    } else {
        hh = bcdToBin(hrReg & 0x3F);
    }

    return true;
}

bool DS3231::setTime(uint8_t hh, uint8_t mm, uint8_t ss) {
    if (!_present) return false;

    _wire.beginTransmission(_addr);
    _wire.write(0x00);
    _wire.write(binToBcd(ss) & 0x7F);
    _wire.write(binToBcd(mm));
    _wire.write(binToBcd(hh) & 0x3F);
    return _wire.endTransmission() == 0;
}

uint8_t DS3231::readReg8(uint8_t reg) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    _wire.endTransmission(false);
    _wire.requestFrom((int)_addr, 1);
    if (_wire.available()) return _wire.read();
    return 0;
}

void DS3231::writeReg8(uint8_t reg, uint8_t val) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    _wire.write(val);
    _wire.endTransmission();
}

uint8_t DS3231::bcdToBin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

uint8_t DS3231::binToBcd(uint8_t bin) {
    return ((bin / 10) << 4) | (bin % 10);
}
