#pragma once
#include <Arduino.h>
#include <Wire.h>

// Minimal I2C driver for the Toshiba/Renesas TC358743 HDMI-to-CSI bridge.
//
// IMPORTANT: We only use this chip for its HDMI front end + InfoFrame/packet
// capture registers. We never touch the CSI-2 video output side, so the init
// sequence below is intentionally smaller than the full Linux driver (which
// also brings up the CSI-2 TX, audio I2S/PDM path, EDID write for full mode
// negotiation, etc.). This means:
//   - HDMI handshake/EDID: we let the chip use its built-in default EDID,
//     which is enough for most sources (incl. GH5) to start outputting video
//     and packets. If the GH5 refuses to lock, you may need to load a known
//     1080p EDID via the EDID registers (see TC358743 datasheet section on
//     EDID RAM) - left as a TODO if needed.
//   - This is a starting point verified against the register map, but has
//     NOT been tested against real silicon by this generator. Budget time
//     for bring-up/debugging against your specific breakout board.
class TC358743 {
public:
    explicit TC358743(TwoWire &wire = Wire, uint8_t addr = 0x0F);

    // Initializes I2C and brings the chip's HDMI receiver + packet capture
    // path up. Returns true if the chip responds with a sane CHIPID.
    bool begin(int sda_pin, int scl_pin, int reset_pin = -1);

    // True once the chip reports a stable TMDS clock (i.e. HDMI cable is
    // connected and the source is outputting a signal).
    bool hasSignal();

    // True if the current signal is HDMI (vs DVI) - HDMI is required for
    // InfoFrames/packets to be present at all.
    bool isHdmiMode();

    // Selects which packet type appears in the rotating PK_ACP_0HEAD buffer.
    void selectPacketType(uint8_t packetType);

    // Raw register access (exposed for the reverse-engineering dump tool).
    uint8_t  readReg8(uint16_t reg);
    uint16_t readReg16(uint16_t reg);
    void     writeReg8(uint16_t reg, uint8_t val);
    void     writeReg16(uint16_t reg, uint16_t val);
    void     readBlock(uint16_t reg, uint8_t *buf, size_t len);

private:
    TwoWire &_wire;
    uint8_t _addr;

    void writeRegRaw(uint16_t reg, const uint8_t *data, size_t len);
};
