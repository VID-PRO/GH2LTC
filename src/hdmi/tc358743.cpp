#include "tc358743.h"
#include "tc358743_regs.h"

TC358743::TC358743(TwoWire &wire, uint8_t addr) : _wire(wire), _addr(addr) {}

// EDID: block 0 (1080p25 DTD, 74.25 MHz) + CEA-861 extension with HDMI VSDB + SVD
// Declares HDMI sink, physical address 1.0.0.0
// SVDs: VIC33(1080p25,native), VIC34(1080p30), VIC32(1080p24), VIC16(1080p60), VIC31(1080i50)
const uint8_t EDID_1080P25[256] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x83, 0x3A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x04, 0x88, 0x20, 0x12, 0x78,
    0xEA, 0x0D, 0xC5, 0xA4, 0x56, 0x4A, 0x9A, 0x27,
    0x12, 0x50, 0x54, 0x20, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1D,
    0x80, 0xD0, 0x72, 0x38, 0x2D, 0x40, 0x10, 0x2C,
    0x45, 0x80, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x0B,
    0x00, 0x00, 0x00, 0xFC, 0x00, 0x54, 0x43, 0x2D,
    0x4C, 0x54, 0x43, 0x2D, 0x48, 0x44, 0x4D, 0x49,
    0x20, 0x20, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xA0,
    0x02, 0x03, 0x00, 0x00, 0x25, 0xA1, 0x22, 0x20,
    0x10, 0x1F, 0x46, 0x03, 0x0C, 0x00, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F,
};

bool TC358743::begin(int sda_pin, int scl_pin, int reset_pin) {
    Serial.printf("TC358743::begin(sda=%d, scl=%d, rst=%d, addr=0x%02X)\n",
                  sda_pin, scl_pin, reset_pin, _addr);
    if (reset_pin >= 0) {
        pinMode(reset_pin, OUTPUT);
        digitalWrite(reset_pin, LOW);
        delay(10);
        digitalWrite(reset_pin, HIGH);
        delay(50);
    }

    _wire.begin(sda_pin, scl_pin, 100000);
    Serial.printf("Wire.begin(%d,%d) done, probing 0x%02X... ", sda_pin, scl_pin, _addr);
    _wire.beginTransmission(_addr);
    if (_wire.endTransmission() != 0) {
        Serial.println(F("no ACK"));
        _wire.end();
        return false;
    }
    Serial.println(F("ACK!"));
    delay(10);

    // Read CHIPID as two byte-wide reads for cross-check
    uint8_t chipLo = readReg8(0x0000);
    uint8_t chipHi = readReg8(0x0001);
    uint16_t chipId = (uint16_t)chipLo | ((uint16_t)chipHi << 8);
    Serial.print(F("CHIPID=0x"));
    Serial.print(chipId, HEX);
    Serial.print(F(" (lo=0x"));
    Serial.print(chipLo, HEX);
    Serial.print(F(" hi=0x"));
    Serial.print(chipHi, HEX);
    Serial.print(F(")"));
    if ((chipId & 0xFF00) == 0x4400) {
        Serial.println(F(" (TC358743)"));
    } else if (chipId == 0xFFFF) {
        Serial.println(F(" (all-0xFFFF — chip not responding)"));
    } else {
        Serial.println(F(" (unknown — may be clone or wiring issue)"));
    }

    writeReg8(HPD_CTL, 0x00);   // HPD low during entire init + EDID write

    // ------ Linux driver `tc358743_initial_setup` equivalent ------

    // Reset IR and CEC blocks (SYSCTL is at 0x0002, 16-bit)
    modifyReg16(SYSCTL, MASK_IRRST | MASK_CECRST, MASK_IRRST | MASK_CECRST);

    // Pulse TX and HDMI reset (assert then deassert)
    pulseReg16(SYSCTL, MASK_CTXRST | MASK_HDMIRST);

    // Wake from sleep mode
    modifyReg16(SYSCTL, MASK_SLEEP, 0);

    // FIFO level (Linux driver: pdata->fifo_level, default = 374)
    writeReg16(FIFOCTL, 374);

    // == Ref clock: 42 MHz (confirmed by chip's power-on defaults — SF=0x1068) ==
    const uint32_t REFCLK_HZ = 42000000;
    uint16_t sys_freq = REFCLK_HZ / 10000;  // 4200 = 0x1068
    writeReg8(SYS_FREQ0, sys_freq & 0xFF);
    writeReg8(SYS_FREQ1, (sys_freq >> 8) & 0xFF);

    // PHY_CTL0: bit 0 = 1 for 42 MHz; preserve default bits 1+5 (clone default 0x22)
    writeReg8(PHY_CTL0, 0x23);

    // CLKM_CTL: bit 0 = PLL_REF_FREQ — 0=27MHz, 1=26/42MHz (MISSING BEFORE!)
    writeReg8(CLKM_CTL, 0x01);

    // FH_MIN = refclk_hz / 100000 = 420
    uint16_t fh_min = REFCLK_HZ / 100000;
    writeReg8(FH_MIN0, fh_min & 0xFF);
    writeReg8(FH_MIN1, (fh_min >> 8) & 0xFF);

    // FH_MAX = fh_min * 66 / 10 = 2772 = 0x0AF4
    uint16_t fh_max = (fh_min * 66) / 10;
    writeReg8(FH_MAX0, fh_max & 0xFF);
    writeReg8(FH_MAX1, (fh_max >> 8) & 0xFF);

    // NCO_F0_MOD: 0x00 for 26/42 MHz, 0x01 for 27 MHz
    writeReg8(NCO_F0_MOD, 0x00);

    // LOCKDET_REF = refclk_hz / 100 = 420000 = 0x668A0
    uint32_t lockdet_ref = REFCLK_HZ / 100;
    writeReg8(LOCKDET_REF0, lockdet_ref & 0xFF);
    writeReg8(LOCKDET_REF1, (lockdet_ref >> 8) & 0xFF);
    writeReg8(LOCKDET_REF2, (lockdet_ref >> 16) & 0xFF);

    // DDC setup — 100ms DDC5V debounce (Linux driver default)
    writeReg8(DDC_CTL, 0x02);

    // EDID mode — E-DDC only (Linux driver: MASK_EDID_MODE_E_DDC = 0x02)
    writeReg8(EDID_MODE, MASK_EDID_MODE_E_DDC);

    // ------ HDMI PHY setup (matching driver's `tc358743_set_hdmi_phy`) ------
    writeReg8(PHY_EN, 0x00);  // disable PHY first

    // PHY_CTL1: SET_PHY_AUTO_RST1_US(1600) = (1600/200)<<4 = 0x80,
    //           SET_FREQ_RANGE_MODE_CYCLES(1) = 0x00
    writeReg8(PHY_CTL1, 0x80);

    // SYS_CLK: clock divider = 1, no differential delay
    writeReg8(SYS_CLK, 0x00);

    // PHY_BIAS: bias current for TMDS receiver
    writeReg8(PHY_BIAS, 0x50);

    // PHY_CSQ: signal-quality count threshold (lower = more sensitive)
    writeReg8(PHY_CSQ, 0x02);

    // PHY_RST: digital reset to latch new PHY config (MUST be set before AVM_CTL/PHY_EN)
    writeReg8(PHY_RST, 0x01);

    // AVM_CTL: Linux driver uses 45 for all refclk frequencies (42 MHz too)
    writeReg8(AVM_CTL, 45);

    // HDMI_DET: async 25ms detection delay
    writeReg8(HDMI_DET, 0x10);

    // HV_RST: no auto reset on HSYNC/VSYNC out of range
    writeReg8(HV_RST, 0x00);

    // Re-enable PHY
    writeReg8(PHY_EN, 0x01);

    // ANA_CTL: normal PCSX mode + analog on
    writeReg8(ANA_CTL, 0x33);

    // VI_MODE: all CE/IT formats detected as RGB full range in DVI mode
    writeReg8(VI_MODE, 0x00);

    // VOUT_SET2: color mode auto
    writeReg8(VOUT_SET2, 0x01);  // MASK_VOUTCOLORMODE_AUTO

    // VOUT_SET3: ext count
    writeReg8(VOUT_SET3, 0x08);  // MASK_VOUT_EXTCNT

    // Read back and log all PHY/ref-clock values for comparison
    {
        Serial.print(F("Init: SF="));
        Serial.print(readReg8(SYS_FREQ1) << 8 | readReg8(SYS_FREQ0), HEX);
        Serial.print(F(" FH="));
        Serial.print(readReg8(FH_MIN1) << 8 | readReg8(FH_MIN0), HEX);
        Serial.print('-');
        Serial.print(readReg8(FH_MAX1) << 8 | readReg8(FH_MAX0), HEX);
        Serial.print(F(" LD="));
        Serial.print((uint32_t)readReg8(LOCKDET_REF2) << 16 | 
                     (uint32_t)readReg8(LOCKDET_REF1) << 8 | readReg8(LOCKDET_REF0), HEX);
        Serial.print(F(" NCO=0x"));
        Serial.print(readReg8(NCO_F0_MOD), HEX);
        Serial.print(F(" PHYCTL0=0x"));
        Serial.print(readReg8(PHY_CTL0), HEX);
        Serial.print(F(" CLKM_CTL=0x"));
        Serial.print(readReg8(CLKM_CTL), HEX);
        Serial.print(F(" PHYCTL1=0x"));
        Serial.print(readReg8(PHY_CTL1), HEX);
        Serial.print(F(" BIAS=0x"));
        Serial.print(readReg8(PHY_BIAS), HEX);
        Serial.print(F(" CSQ=0x"));
        Serial.print(readReg8(PHY_CSQ), HEX);
        Serial.print(F(" AVM=0x"));
        Serial.print(readReg8(AVM_CTL), HEX);
        Serial.print(F(" PHY_EN=0x"));
        Serial.print(readReg8(PHY_EN), HEX);
        Serial.print(F(" ANA_CTL=0x"));
        Serial.print(readReg8(ANA_CTL), HEX);
        Serial.println();
    }

    // ------ Write EDID (must be done BEFORE INIT_END, per TC358743 datasheet) ------

    // 1. Disable EDID access while writing
    writeReg8(EDID_MODE, 0x00);
    delay(5);
    bool edidOk = writeEdidByteByByte(EDID_1080P25, sizeof(EDID_1080P25));
    Serial.print(F("EDID write: "));
    Serial.println(edidOk ? F("OK") : F("FAILED"));

    // 2. EDID reshow / re-latch (per Linux driver edid_reshow)
    writeReg8(EDID_LEN1, 0x00);
    writeReg8(EDID_LEN2, 0x00);
    writeReg8(EDID_SEG_NUM, 0x00);
    writeReg8(EDID_LEN1, sizeof(EDID_1080P25) & 0xFF);
    writeReg8(EDID_LEN2, (sizeof(EDID_1080P25) >> 8) & 0xFF);

    // 3. Re-enable EDID access (E-DDC, matching Linux driver)
    writeReg8(EDID_MODE, MASK_EDID_MODE_E_DDC);
    delay(5);

    writeReg8(INIT_END, 0x01);
    delay(100);

    // 4. Enable HPD
    writeReg8(HPD_CTL, 0x01);
    delay(150);

    // 5. Configure CSI-2 TX (2 lanes, continuous clock)
    configureCsiTx();

    return true;
}

bool TC358743::hasSignal() {
    uint8_t status = readReg8(SYS_STATUS);
    return status & 0x02;
}

bool TC358743::isHdmiMode() {
    uint8_t status = readReg8(SYS_STATUS);
    return status & 0x10;
}

void TC358743::selectPacketType(uint8_t packetType) {
    writeReg8(TYP_ACP_SET, packetType);
}

bool TC358743::writeEdid(const uint8_t *data, size_t len) {
    if (len == 0 || len > 256) return false;

    // Match Linux driver order: set length first, then write data, don't touch EDID_MODE
    writeReg8(EDID_LEN1, len & 0xFF);
    writeReg8(EDID_LEN2, (len >> 8) & 0xFF);

    // EDID RAM is 1024 bytes starting at 0x8C00.
    // I2C max transfer per the Linux driver is 128 data bytes (+2 reg addr).
    // Write 128-byte blocks sequentially.
    static const size_t BLOCK = 128;
    for (size_t off = 0; off < len; off += BLOCK) {
        size_t chunk = (off + BLOCK <= len) ? BLOCK : (len - off);
        uint16_t reg = EDID_RAM + off;

        _wire.beginTransmission(_addr);
        _wire.write((uint8_t)(reg >> 8));
        _wire.write((uint8_t)(reg & 0xFF));
        for (size_t i = 0; i < chunk; i++) {
            _wire.write(data[off + i]);
        }
        uint8_t err = _wire.endTransmission();
        if (err != 0) return false;
    }

    return true;
}

bool TC358743::writeEdidByteByByte(const uint8_t *data, size_t len) {
    if (len == 0 || len > 256) return false;

    writeReg8(EDID_LEN1, len & 0xFF);
    writeReg8(EDID_LEN2, (len >> 8) & 0xFF);

    for (size_t i = 0; i < len; i++) {
        writeReg8(EDID_RAM + i, data[i]);
    }

    return true;
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
    // Use repeated START (no STOP) between address write and data read,
    // matching the Linux kernel driver's combined I2C transaction format.
    // The TC358743 requires this for reliable 0x85xx register page access.
    for (size_t i = 0; i < len; i++) {
        _wire.beginTransmission(_addr);
        _wire.write((uint8_t)((reg + i) >> 8));
        _wire.write((uint8_t)((reg + i) & 0xFF));
        _wire.endTransmission(false);
        _wire.requestFrom((int)_addr, 1);
        if (_wire.available()) {
            buf[i] = (uint8_t)_wire.read();
        }
    }
}

void TC358743::writeReg32(uint16_t reg, uint32_t val) {
    uint8_t buf[4] = {
        (uint8_t)(val & 0xFF),
        (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF),
        (uint8_t)((val >> 24) & 0xFF)
    };
    writeRegRaw(reg, buf, 4);
}

// -------------------------------------------------------------------
// CSI-2 TX configuration
// -------------------------------------------------------------------
void TC358743::configureCsiTx() {
    // Enable all clock/data lanes (clear lane-disable bits)
    writeReg16(CLW_CNTRL, 0x0000);
    writeReg16(D0W_CNTRL, 0x0000);
    writeReg16(D1W_CNTRL, 0x0000);
    // D2W_CNTRL / D3W_CNTRL unused for 2-lane, but enable them anyway
    writeReg16(D2W_CNTRL, 0x0000);
    writeReg16(D3W_CNTRL, 0x0000);

    // Enable HS TX VREG for clock lane and data lanes 0-1
    writeReg32(HSTXVREGEN,
               MASK_CLM_HSTXVREGEN | MASK_D0M_HSTXVREGEN | MASK_D1M_HSTXVREGEN);

    // Reset CSI configuration via CSI_CONFW
    writeReg32(CSI_CONFW, MASK_MODE_CLEAR | MASK_ADDRESS_CSI_CONTROL | 0);

    // Set CSI_CONTROL:
    //   CSI_MODE – enable CSI block
    //   TXHSMD   – HS transmit mode
    //   HSCKMD   – HS clock mode
    //   NOL_2    – 2 data lanes
    //   EOTDIS   – disable EOT packet (some receivers need this)
    writeReg32(CSI_CONFW,
               MASK_MODE_SET | MASK_ADDRESS_CSI_CONTROL |
               MASK_CSI_MODE | MASK_TXHSMD | MASK_HSCKMD |
               MASK_NOL_2 | MASK_EOTDIS);

    // Enable CSI interrupts (halt + error)
    writeReg32(CSI_CONFW,
               MASK_MODE_SET | MASK_ADDRESS_CSI_INT_ENA |
               MASK_IENHLT | MASK_IENER);

    // Set continuous clock mode for the D-PHY
    writeReg32(TXOPTIONCNTRL, MASK_CONTCLKMODE);

    // Start the D-PHY state machine
    writeReg32(STARTCNTRL, MASK_START);

    Serial.println(F("CSI-2 TX: 2 lanes, continuous clock, started"));
}

void TC358743::enableCsiStream(bool enable) {
    if (enable) {
        writeReg32(TXOPTIONCNTRL, 0);
        writeReg32(TXOPTIONCNTRL, MASK_CONTCLKMODE);
        writeReg8(VI_MUTE, MASK_AUTO_MUTE);
        writeReg32(CSI_START, MASK_STRT);
        Serial.println(F("CSI-2 TX: stream ON"));
    } else {
        writeReg8(VI_MUTE, MASK_AUTO_MUTE | MASK_VI_MUTE);
        Serial.println(F("CSI-2 TX: stream OFF"));
    }
}

void TC358743::pulseReg16(uint16_t reg, uint16_t mask) {
    uint16_t val = readReg16(reg);
    writeReg16(reg, val | mask);
    writeReg16(reg, val & ~mask);
}

void TC358743::modifyReg16(uint16_t reg, uint16_t clearMask, uint16_t setMask) {
    uint16_t val = readReg16(reg);
    val &= ~clearMask;
    val |= setMask;
    writeReg16(reg, val);
}

void TC358743::reapplyPhy() {
    writeReg8(PHY_BIAS, 0x50);
    writeReg8(PHY_CSQ, 0x04);
    Serial.println(F("PHY: re-applied (BIAS=0x50 CSQ=0x04)"));
}

