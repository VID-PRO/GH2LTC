#pragma once
#include <Arduino.h>
#include <Wire.h>

class DS3231 {
public:
    DS3231(TwoWire &wire = Wire, uint8_t addr = 0x68);

    bool begin(int sda_pin, int scl_pin);
    bool isPresent();

    bool readTime(uint8_t &hh, uint8_t &mm, uint8_t &ss);
    bool setTime(uint8_t hh, uint8_t mm, uint8_t ss);

    static uint8_t bcdToBin(uint8_t bcd);
    static uint8_t binToBcd(uint8_t bin);

private:
    TwoWire &_wire;
    uint8_t _addr;
    bool _present;

    uint8_t readReg8(uint8_t reg);
    void writeReg8(uint8_t reg, uint8_t val);
};
