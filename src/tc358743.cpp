#include "tc358743.h"
#include "tc358743_regs.h"

TC358743::TC358743(TwoWire &wire, uint8_t addr) : _wire(wire), _addr(addr) {}

// -----------------------------------------------------------------------
// Bit-bang I2C address probe — avoids initialising the hardware I2C
// peripheral (and the GPIO noise that comes with it) when no device is
// present.  On ESP32-C3, even idle I2C pins with weak internal pull-ups
// can couple enough noise to degrade WiFi association sensitivity.
// -----------------------------------------------------------------------
static bool bitBangProbe(int sda, int scl, uint8_t addr) {
    // Start with pull-ups enabled so pins sit at a known high level.
    pinMode(scl, INPUT_PULLUP);
    pinMode(sda, INPUT_PULLUP);
    delayMicroseconds(10);

    // --- START condition (SDA ↓ while SCL ↑) ---
    pinMode(sda, OUTPUT_OPEN_DRAIN);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    pinMode(scl, OUTPUT_OPEN_DRAIN);
    digitalWrite(scl, LOW);
    delayMicroseconds(5);

    // Send 7-bit address + R/W=0, MSB first
    uint8_t byte = (addr << 1) | 0x00;
    for (int i = 0; i < 8; i++) {
        digitalWrite(sda, (byte & 0x80) ? HIGH : LOW);
        byte <<= 1;
        delayMicroseconds(3);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
        digitalWrite(scl, LOW);
        delayMicroseconds(3);
    }

    // Release SDA for slave ACK
    pinMode(sda, INPUT_PULLUP);
    delayMicroseconds(3);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    bool ack = (digitalRead(sda) == LOW);
    digitalWrite(scl, LOW);
    delayMicroseconds(3);

    // --- STOP condition (SDA ↑ while SCL ↑) ---
    pinMode(sda, OUTPUT_OPEN_DRAIN);
    digitalWrite(sda, LOW);
    delayMicroseconds(3);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda, HIGH);
    delayMicroseconds(5);

    // Restore pins to safe pulled-up input
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);

    return ack;
}

bool TC358743::begin(int sda_pin, int scl_pin, int reset_pin) {
    if (reset_pin >= 0) {
        pinMode(reset_pin, OUTPUT);
        digitalWrite(reset_pin, LOW);
        delay(10);
        digitalWrite(reset_pin, HIGH);
        delay(50);
    }

    // Bit-bang probe first — only initialise the hardware I2C peripheral if
    // we actually found a device.  This keeps SDA/SCL silent (INPUT_PULLUP)
    // when nothing is connected, avoiding potential noise coupling into the
    // WiFi radio on ESP32-C3.
    if (!bitBangProbe(sda_pin, scl_pin, _addr)) {
        return false;
    }

    _wire.begin(sda_pin, scl_pin, 100000);

    uint16_t chipId = readReg16(CHIPID);
    if ((chipId & 0xFF00) != 0x4400) {
        return false;
    }

    writeReg16(SYS_FREQ0, 5400);
    writeReg8(HDMI_DET, 0x0F);
    writeReg8(DDC_CTL, 0x01);
    writeReg8(HPD_CTL, 0x01);
    writeReg8(ANA_CTL, 0x23);
    writeReg8(AVM_CTL, 0x02);

    writeReg8(PHY_BIAS, 0x40);
    writeReg8(PHY_CSQ, 0x3C);
    writeReg16(PHY_CTL0, 0x0000);
    writeReg8(PHY_EN, 0x01);

    writeReg8(INIT_END, 0x01);

    delay(100);

    return true;
}

bool TC358743::hasSignal() {
    uint8_t status = readReg8(SYS_STATUS);
    return status & 0x01;
}

bool TC358743::isHdmiMode() {
    uint8_t status = readReg8(SYS_STATUS);
    return status & 0x02;
}

void TC358743::selectPacketType(uint8_t packetType) {
    writeReg8(TYP_ACP_SET, packetType);
}

void TC358743::writeRegRaw(uint16_t reg, const uint8_t *data, size_t len) {
    _wire.beginTransmission(_addr);
    _wire.write((uint8_t)(reg >> 8));
    _wire.write((uint8_t)(reg & 0xFF));
    for (size_t i = 0; i < len; i++) _wire.write(data[i]);
    _wire.endTransmission();
}

void TC358743::writeReg8(uint16_t reg, uint8_t val) {
    writeRegRaw(reg, &val, 1);
}

void TC358743::writeReg16(uint16_t reg, uint16_t val) {
    uint8_t buf[2] = { (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
    writeRegRaw(reg, buf, 2);
}

uint8_t TC358743::readReg8(uint16_t reg) {
    uint8_t val = 0;
    readBlock(reg, &val, 1);
    return val;
}

uint16_t TC358743::readReg16(uint16_t reg) {
    uint8_t buf[2] = {0, 0};
    readBlock(reg, buf, 2);
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

void TC358743::readBlock(uint16_t reg, uint8_t *buf, size_t len) {
    _wire.beginTransmission(_addr);
    _wire.write((uint8_t)(reg >> 8));
    _wire.write((uint8_t)(reg & 0xFF));
    _wire.endTransmission(false);
    _wire.requestFrom((int)_addr, (int)len);
    size_t i = 0;
    while (_wire.available() && i < len) {
        buf[i++] = _wire.read();
    }
}
