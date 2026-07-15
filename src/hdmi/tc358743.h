#pragma once
#include <Arduino.h>
#include <Wire.h>

// I2C driver for the Toshiba/Renesas TC358743 HDMI-to-CSI-2 bridge.
// Includes HDMI RX init (EDID, PHY, DDC) and CSI-2 TX configuration
// (2-lane, continuous clock, RGB/YUV output).

extern const uint8_t EDID_1080P25[256];

class TC358743 {
public:
    explicit TC358743(TwoWire &wire = Wire, uint8_t addr = 0x0F);

    bool begin(int sda_pin, int scl_pin, int reset_pin = -1);

    bool hasSignal();
    bool isHdmiMode();
    void selectPacketType(uint8_t packetType);

    void configureCsiTx();
    void enableCsiStream(bool enable);
    void configurePhy(uint8_t phyCtl0);

    bool writeEdid(const uint8_t *data, size_t len);
    bool writeEdidByteByByte(const uint8_t *data, size_t len);

    uint8_t  readReg8(uint16_t reg);
    uint16_t readReg16(uint16_t reg);
    void     writeReg8(uint16_t reg, uint8_t val);
    void     writeReg16(uint16_t reg, uint16_t val);
    void     writeReg32(uint16_t reg, uint32_t val);
    void     readBlock(uint16_t reg, uint8_t *buf, size_t len);
    void     modifyReg8(uint16_t reg, uint8_t clearMask, uint8_t setMask);

    // Pulse a mask bit in a 16-bit register: assert then deassert
    void pulseReg16(uint16_t reg, uint16_t mask);
    // Read-modify-write: clear then set bits in a 16-bit register
    void modifyReg16(uint16_t reg, uint16_t clearMask, uint16_t setMask);

private:
    TwoWire &_wire;
    uint8_t _addr;

    void writeRegRaw(uint16_t reg, const uint8_t *data, size_t len);
};
