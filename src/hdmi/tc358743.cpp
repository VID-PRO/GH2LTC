#include "tc358743.h"
#include "tc358743_regs.h"

TC358743::TC358743(TwoWire &wire, uint8_t addr) : _wire(wire), _addr(addr) {}

// Forward declaration
static void probePD3400();

// p4kvm-known-good EDID from Waveshare HDMI-to-CSI adapter (1080p30)
static const uint8_t EDID_1080P30_25[256] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x52, 0x62, 0x88, 0x88, 0x00, 0x88, 0x88, 0x88,
    0x1c, 0x15, 0x01, 0x03, 0x80, 0xa0, 0x5a, 0x78, 0x0a, 0x0d, 0xc9, 0xa0, 0x57, 0x47, 0x98, 0x27,
    0x12, 0x48, 0x4c, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1d, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
    0x45, 0x00, 0x80, 0x38, 0x74, 0x00, 0x00, 0x1e, 0x01, 0x1d, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40,
    0x58, 0x2c, 0x45, 0x00, 0x80, 0x38, 0x74, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x44,
    0x43, 0x44, 0x5a, 0x2d, 0x48, 0x32, 0x43, 0x20, 0x4d, 0x4f, 0x44, 0x0a, 0x00, 0x00, 0x00, 0xfd,
    0x00, 0x14, 0x78, 0x01, 0xff, 0x10, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xb9,
    0x02, 0x03, 0x1a, 0x71, 0x47, 0xa2, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x23, 0x09, 0x07, 0x01,
    0x83, 0x01, 0x00, 0x00, 0x65, 0x03, 0x0c, 0x00, 0x10, 0x00, 0x01, 0x1d, 0x80, 0x18, 0x71, 0x38,
    0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00, 0x80, 0x38, 0x74, 0x00, 0x00, 0x1e, 0x01, 0x1d, 0x80, 0x18,
    0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00, 0x80, 0x38, 0x74, 0x00, 0x00, 0x1e, 0x01, 0x1d,
    0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00, 0x80, 0x38, 0x74, 0x00, 0x00, 0x1e,
    0x01, 0x1d, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00, 0x80, 0x38, 0x74, 0x00,
    0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
};

// ---------- 8-bit read-modify-write ----------
void TC358743::modifyReg8(uint16_t reg, uint8_t clearMask, uint8_t setMask) {
    uint8_t val = readReg8(reg);
    val = (val & ~clearMask) | setMask;
    writeReg8(reg, val);
}

bool TC358743::begin(int sda_pin, int scl_pin, int reset_pin, uint32_t refclk_hz) {

    Serial.printf("TC358743::begin(sda=%d, scl=%d, rst=%d, addr=0x%02X)\n",
                  sda_pin, scl_pin, reset_pin, _addr);

    // ---- hardware reset ----
    if (reset_pin >= 0) {
        pinMode(reset_pin, OUTPUT);
        digitalWrite(reset_pin, LOW);
        delay(100);
        digitalWrite(reset_pin, HIGH);
        delay(100);
    }

    _wire.begin(sda_pin, scl_pin, 100000);
    Serial.printf("Wire.begin(%d,%d) done, probing 0x%02X... ", sda_pin, scl_pin, _addr);
    _wire.beginTransmission(_addr);
    if (_wire.endTransmission() != 0) {
        Serial.println(F("no ACK"));
        // Do NOT call _wire.end() here.  Leaving the bus initialised avoids a
        // create/delete/recreate cycle that can leave the NG I2C HAL on
        // ESP32‑C3 in ESP_ERR_INVALID_STATE for subsequent devices (OLED).
        // _wire.end();
        return false;
    }
    Serial.println(F("ACK!"));
    delay(10);

    // ---- I2C bus scan ----
    Serial.println(F("I2C bus scan (0x03..0x77):"));
    for (uint8_t a = 0x03; a < 0x78; a++) {
        _wire.beginTransmission(a);
        if (_wire.endTransmission() == 0) {
            Serial.printf("  0x%02X found\n", a);
        }
    }

    // ---- read CHIPID ----
    uint8_t chipLo = readReg8(CHIPID);
    uint8_t chipHi = readReg8(CHIPID + 1);
    uint16_t chipId = (uint16_t)chipLo | ((uint16_t)chipHi << 8);
    Serial.printf("CHIPID=0x%04X (lo=0x%02X hi=0x%02X)\n", chipId, chipLo, chipHi);

    // ---- detect refclk ----
    uint8_t sfLo = readReg8(SYS_FREQ0);
    uint8_t sfHi = readReg8(SYS_FREQ1);
    uint16_t sysFreq = (uint16_t)sfLo | ((uint16_t)sfHi << 8);
    Serial.printf("SYS_FREQ (power-on): 0x%04X (%u)\n", sysFreq, sysFreq);
    _refclk_hz = refclk_hz ? refclk_hz :
        ((sysFreq >= 4000) ? 42000000u : 27000000u);
    const uint32_t REFCLK_HZ = _refclk_hz;
    Serial.printf("REFCLK_HZ: %u (%.1f MHz)%s\n", REFCLK_HZ,
                  REFCLK_HZ / 1000000.0f,
                  refclk_hz ? " (override)" : "");

    // ============================================================
    //  p4kvm-style init sequence (Linux kernel derived)
    //  HPD stays LOW throughout; raised only at the very end.
    //  Register map: DDC_CTL=0x8543, HPD_CTL=0x8544,
    //                EDID_MODE=0x85C7 (E‑DDC mode = 0x02)
    // ============================================================

    // ---- HPD low (source must not DDC during init) ----
    writeReg8(HPD_CTL, 0x00);
    delay(20);

    // ---- CLKM pad power cycle (kernel init_hdmi: 0x8562 = 0x10/0x00) ----
    writeReg8(CLKM_CTL, 0x10);    // power down clock management pads
    delay(10);
    writeReg8(CLKM_CTL, 0x00);    // power up (PLL_REF_FREQ=0 for 27 MHz)

    // ---- SYSCTL: reset blocks, wake up ----
    // Set IRRST + CECRST (put IR and CEC in reset)
    modifyReg16(SYSCTL, MASK_IRRST | MASK_CECRST, MASK_IRRST | MASK_CECRST);
    // Pulse CTXRST + HDMIRST
    writeReg16(SYSCTL, readReg16(SYSCTL) | (MASK_CTXRST | MASK_HDMIRST));
    writeReg16(SYSCTL, readReg16(SYSCTL) & ~(MASK_CTXRST | MASK_HDMIRST));
    delay(1);
    // Clear SLEEP
    modifyReg16(SYSCTL, MASK_SLEEP, 0);

    // ---- FIFO level ----
    writeReg16(FIFOCTL, 374);

    // ---- Reference clock registers ----
    uint16_t sf = REFCLK_HZ / 10000;
    writeReg8(SYS_FREQ0, sf & 0xFF);
    writeReg8(SYS_FREQ1, (sf >> 8) & 0xFF);

    modifyReg8(PHY_CTL0, 0x02,
               (REFCLK_HZ == 42000000) ? 0x02 : 0x00);

    // CLKM_CTL (0x8562) intentionally NOT written — not in kernel header;
    // may alias to HDCP/other reserved register space. Kernel relies solely
    // on PHY_CTL0 bit 1 (MASK_PHY_SYSCLK_IND) for refclk >27 MHz.

    uint16_t fh_min = REFCLK_HZ / 100000;
    writeReg8(FH_MIN0, fh_min & 0xFF);
    writeReg8(FH_MIN1, (fh_min >> 8) & 0xFF);
    uint16_t fh_max = (fh_min * 66u) / 10u;
    writeReg8(FH_MAX0, fh_max & 0xFF);
    writeReg8(FH_MAX1, (fh_max >> 8) & 0xFF);

    uint32_t ld = REFCLK_HZ / 100;
    writeReg8(LOCKDET_REF0, ld & 0xFF);
    writeReg8(LOCKDET_REF1, (ld >> 8) & 0xFF);
    writeReg8(LOCKDET_REF2, (ld >> 16) & 0xFF);

    modifyReg8(NCO_F0_MOD, 0x03, (REFCLK_HZ == 27000000) ? 0x01 : 0x00);

    // ---- CEC clock (p4kvm set_ref_clk) ----
    {
        uint32_t cec = (656u * sf) / 4200u;
        writeReg16(0x0028, (uint16_t)cec);
        writeReg16(0x002a, (uint16_t)cec);
    }

    // ---- CSI-TX PLL (configure BEFORE PHY, matching kernel order) ----
    {
        uint16_t pll_prd = REFCLK_HZ / 6000000;
        uint16_t pll_fbd = 972000000u / (REFCLK_HZ / pll_prd);
        uint32_t hsck = (REFCLK_HZ / pll_prd) * pll_fbd;
        uint16_t pll_frs;
        if (hsck > 500000000)      pll_frs = 0;
        else if (hsck > 250000000) pll_frs = 1;
        else if (hsck > 125000000) pll_frs = 2;
        else                       pll_frs = 3;
        uint16_t pllctl0_new = ((pll_prd - 1) << 12) | (pll_fbd - 1);
        if ((readReg16(PLLCTL0) != pllctl0_new) || ((readReg16(PLLCTL1) & 0x0001) == 0)) {
            // Set dividers first, then enable PLL (kernel order)
            writeReg16(PLLCTL0, pllctl0_new);
            writeReg16(PLLCTL1, (pll_frs << 10) | 0x0003); // FRS | RESETB | PLL_EN
            delay(1);
            modifyReg16(PLLCTL1, MASK_CKEN, MASK_CKEN);    // CK_EN
        }
    }
    // Re-apply SYSCTL reset bits after possible PLL register aliasing
    modifyReg16(SYSCTL, MASK_IRRST | MASK_CECRST, MASK_IRRST | MASK_CECRST);
    writeReg16(SYSCTL, readReg16(SYSCTL) | (MASK_CTXRST | MASK_HDMIRST));
    writeReg16(SYSCTL, readReg16(SYSCTL) & ~(MASK_CTXRST | MASK_HDMIRST));

    // ---- DDC + EDID mode (wisape pattern: DDC5V comp + active detect delay) ----
    writeReg8(DDC_CTL, 0x32);                 // [5:4]=0b11 5V comp, [1:0]=0b10 DDC5V detect delay 100ms
    writeReg8(EDID_MODE, MASK_EDID_MODE_DDC2B | MASK_EDID_MODE_E_DDC); // DDC2B + E‑DDC

    // ---- HDCP: force manual auth (off) ----
    writeReg8(HDCP_MODE, MASK_HDCP_MANUAL_AUTH);

    // ---- Audio init (p4kvm set_hdmi_audio) ----
    writeReg8(FORCE_MUTE, 0x00);
    writeReg8(AUTO_CMD0, 0xF3);
    writeReg8(AUTO_CMD1, 0x02);
    writeReg8(AUTO_CMD2, 0x0C);
    writeReg8(BUFINIT_START, 0xF4);           // 500ms / 2
    writeReg8(FS_MUTE, 0x00);
    writeReg8(FS_IMODE, 0x03);
    writeReg8(ACR_MODE, 0x02);
    writeReg8(ACR_MDF0, 0x30);
    writeReg8(ACR_MDF1, 0x07);
    writeReg8(SDO_MODE1, 0x02);
    writeReg8(DIV_MODE, 100);
    modifyReg16(CONFCTL, 0, 0x0C14);

    // ---- InfoFrame packet limits (p4kvm set_hdmi_info_frame) ----
    writeReg8(PK_INT_MODE, 0xFF);
    writeReg8(NO_PKT_LIMIT, 0x2C);
    writeReg8(NO_PKT_CLR, 0x53);
    writeReg8(ERR_PK_LIMIT, 0x01);
    writeReg8(NO_PKT_LIMIT2, 0x30);
    writeReg8(NO_GDB_LIMIT, 0x10);

    // p4kvm does NOT force HDMI_CTL — leave at power-on default (auto-detect)
    // PHY_FREQ at 0x0812 aliases to PLLCTL1 (0x0022) on this chip variant — do NOT write it.

    // ---- Video output mode ----
    writeReg8(VI_MODE, 0x00);                 // clear RGB_DVI
    writeReg8(VOUT_SET2, 0x01);               // VOUTCOLORMODE_AUTO
    writeReg8(VOUT_SET3, MASK_VOUT_EXTCNT);

    // Log init config
    {
        Serial.print(F("Init: SF="));
        Serial.print(readReg16(SYS_FREQ0), HEX);
        Serial.print(F(" PHY0=0x"));
        Serial.print(readReg8(PHY_CTL0), HEX);
        Serial.print(F(" DDC=0x"));
        Serial.print(readReg8(DDC_CTL), HEX);
        Serial.print(F(" EDID_MODE=0x"));
        Serial.print(readReg8(EDID_MODE), HEX);
        Serial.println();
    }

    // ---- Write EDID byte-by-byte (Wire buffer is ≤128 bytes) ----
    writeReg8(EDID_LEN1, 0x00);
    writeReg8(EDID_LEN2, 0x01);              // 256 bytes
    delay(5);

    Serial.println(F("Writing EDID to SRAM (32-byte chunks)..."));
    for (int i = 0; i < 256; i += 32) {
        writeRegRaw(EDID_RAM + i, EDID_1080P30_25 + i, 32);
    }
    delay(5);

    {
        uint8_t edidFirst = readReg8(EDID_RAM);
        uint8_t edidLast  = readReg8(EDID_RAM + 255);
        uint8_t ck0 = readReg8(EDID_RAM + 127);
        uint8_t ck1 = readReg8(EDID_RAM + 255);
        (void)ck1;
    Serial.printf("EDID verify: first=0x%02X last=0x%02X block0_ck=0x%02X block1_ck=0x%02X %s\n",
                  edidFirst, edidLast, ck0, ck1,
                  (edidFirst == 0x00 && ck0 == 0xB9) ? "OK" : "FAILED");
        uint8_t edidSegNum = readReg8(EDID_SEG_NUM);
        if (edidSegNum != 0) {
            Serial.printf("EDID_SEG_NUM before fix: 0x%02X\n", edidSegNum);
            writeReg8(EDID_SEG_NUM, 0x00);
            delay(5);
            Serial.printf("EDID_SEG_NUM after fix:  0x%02X\n", readReg8(EDID_SEG_NUM));
        }
    }

    // ---- DDC bus reset (kernel does this after EDID write) ----
    writeReg8(0x8556, 0x01);  // DDC_RESET = 1
    delay(1);
    writeReg8(0x8556, 0x00);  // DDC_RESET = 0

    // ---- Pulse HPD after EDID (wisape: HDMI spec requires HPD low 100ms when EDID changes) ----
    {
        writeReg8(HPD_CTL, 0x01);   // HPD high (manual)
        delay(10);
        writeReg8(HPD_CTL, 0x00);   // HPD pulse low
        delay(100);
        // HPD low clears EDID_MODE — re‑enable DDC
        writeReg8(EDID_MODE, MASK_EDID_MODE_DDC2B | MASK_EDID_MODE_E_DDC);
    }

    // ---- enable_stream(false): mute video, disable buffers (p4kvm order) ----
    writeReg8(VI_MUTE, MASK_AUTO_MUTE | MASK_VI_MUTE);   // 0xD0
    modifyReg16(CONFCTL, MASK_VBUFEN | MASK_ABUFEN, 0);

    // ---- CSI‑2 TX (2 lanes, non‑continuous clock) ----
    writeReg32(CLW_CNTRL, 0x0000);
    writeReg32(D0W_CNTRL, 0x0000);
    writeReg32(D1W_CNTRL, 0x0000);
    writeReg32(D2W_CNTRL, 0x0001);   // disable unused lanes
    writeReg32(D3W_CNTRL, 0x0001);

    writeReg32(LINEINITCNT,   0x1B58);
    writeReg32(LPTXTIMECNT,   0x0007);
    writeReg32(TCLK_HEADERCNT, 0x2806);
    writeReg32(TCLK_TRAILCNT, 0x0000);
    writeReg32(THS_HEADERCNT, 0x0806);
    writeReg32(TWAKEUP,       0x4268);
    writeReg32(TCLK_POSTCNT,  0x0008);
    writeReg32(THS_TRAILCNT,  0x0005);
    writeReg32(HSTXVREGCNT,   0x0000);
    writeReg32(HSTXVREGEN,    0x0007);   // CLK + D0 + D1
    writeReg32(TXOPTIONCNTRL, 0x00000000); // non‑continuous clock
    writeReg32(STARTCNTRL, 0x00000001);
    writeReg32(CSI_START, 0x00000001);      // initial start (p4kvm does this before HPD)

    // CSI_CONFW writes (p4kvm order)
    writeReg32(CSI_CONFW, 0xA0038082);   // CSI control: CSI_MODE, TXHSMD, NOL_2
    writeReg32(CSI_CONFW, 0xA0140312);   // ERR_INTENA: INER|WCER|QUNK|TXBRK
    writeReg32(CSI_CONFW, 0xC0150012);   // ERR_HALT: clear TXBRK|QUNK (NOT INER|WCER)
    writeReg32(CSI_CONFW, 0xA0060004);   // INT_ENA: INTER

    // ---- Color space: RGB888 ----
    writeReg8(VOUT_SET2, 0x01);
    writeReg8(VI_REP, 0x00);

    // ---- Interrupts ----
    writeReg16(INTSTATUS, 0xFFFF);
    writeReg16(INTMASK, (uint16_t)~(MASK_HDMI_MSK | MASK_CSI_MSK));

    // ---- Diagnostics before HPD ----
    {
        uint8_t st = readReg8(SYS_STATUS);
        uint8_t hdmiCtl = readReg8(HDMI_CTL);
        uint8_t phyCtl1 = readReg8(PHY_CTL1);
        uint8_t phyCtl2 = readReg8(PHY_CTL2);
        uint8_t phyEn = readReg8(PHY_EN);
        uint8_t anaCtl = readReg8(ANA_CTL);
        uint8_t initEnd = readReg8(INIT_END);
        uint16_t pllctl0 = readReg16(PLLCTL0);
        uint16_t pllctl1 = readReg16(PLLCTL1);
        uint16_t sysctl = readReg16(SYSCTL);
        uint8_t vi1 = readReg8(VI_STATUS1);
        uint8_t clk = readReg8(CLK_STATUS);
        uint8_t vi3 = readReg8(VI_STATUS3);
        uint8_t phyerr = readReg8(PHYERR_STATUS);
        // Measure TMDS clock frequency via FREQ_MON
        writeReg8(0x8690, 0x01);      // FREQ_MON_CTL: start measurement
        delay(5);
        uint16_t freqMon = readReg16(0x8692);  // FREQ_MON_DATA
        uint32_t tmdsClkKhz = (uint32_t)freqMon * (_refclk_hz / 1000) / 65536;
        Serial.printf("Pre-HPD: SYS_STATUS=0x%02X (DDC5V=%d TMDS=%d PLL=%d HDMI=%d)\n",
                      st, (st >> 0) & 1, (st >> 1) & 1, (st >> 2) & 1, (st >> 4) & 1);
        Serial.printf("           CLK_STATUS=0x%02X (clk=%d dclk=%d stable=%d)\n",
                      clk, clk & 1, (clk >> 1) & 1, (clk >> 2) & 1);
        Serial.printf("         HDMI_CTL=0x%02X PHY_CTL1=0x%02X PHY_CTL2=0x%02X PHY_EN=0x%02X\n",
                      hdmiCtl, phyCtl1, phyCtl2, phyEn);
        Serial.printf("         ANA_CTL=0x%02X INIT_END=0x%02X CLKM_CTL=0x%02X\n",
                      anaCtl, initEnd, readReg8(CLKM_CTL));
        Serial.printf("         PLLCTL0=0x%04X PLLCTL1=0x%04X SYSCTL=0x%04X\n",
                      pllctl0, pllctl1, sysctl);
        Serial.printf("         VI1=0x%02X CLK=0x%02X VI3=0x%02X PHYERR=0x%02X\n",
                      vi1, clk, vi3, phyerr);
        Serial.printf("         FREQ_MON=0x%04X TMDS_CLK=%u kHz\n",
                      freqMon, tmdsClkKhz);
    }

    // ---- Enable stream (video FIFO on) then HPD high ----
    writeReg8(VI_MUTE, MASK_AUTO_MUTE);
    modifyReg16(CONFCTL, 0, MASK_VBUFEN | MASK_ABUFEN);
    delay(150);

    // ---- HPD pulse (wisape pattern) then ANA_CTL/INIT_END ----
    writeReg8(HPD_CTL, 0x01);       // exiting interlock: manual high
    delay(20);
    writeReg8(HPD_CTL, 0x00);       // low 100ms
    delay(100);
    writeReg8(HPD_CTL, 0x10);       // interlock mode (follows DDC5V)
    delay(10);
    Serial.printf("HPD_CTL (0x%04X) = 0x%02X\n",
                  HPD_CTL, readReg8(HPD_CTL));

    // HPD low clears EDID_MODE — re‑enable DDC for source EDID reads
    writeReg8(EDID_MODE, MASK_EDID_MODE_DDC2B | MASK_EDID_MODE_E_DDC);

    // ---- HDMI PHY (kernel set_hdmi_phy) — AFTER HPD, matching kernel init_hdmi ----
    writeReg8(PHY_EN, 0x00);                  // disable before config
    writeReg8(PHY_CTL1, 0xA1);                // auto_rst1=1, freq_range=01 (25-80MHz)
    writeReg8(PHY_BIAS, 0x40);
    writeReg8(PHY_CSQ, 0x0a);
    writeReg8(AVM_CTL, 45);
    writeReg8(HDMI_DET, 0xC1);                // TMDS termination + AVD detection
    writeReg8(ANA_CTL, 0x31);                 // DAC/PLL power on
    writeReg8(PHY_EN, 0x01);                  // enable PHY
    writeReg8(INIT_END, 0x01);                // HDMI RX init complete

    writeReg32(CSI_START, 0x00000001);

    // ---- Signal detection ----
    {
        bool signalSeen = false;
        for (int i = 0; i < 100; i++) {
            delay(50);
            uint8_t status = readReg8(SYS_STATUS);
            if ((status & 0x02) || ((status & 0x0C) == 0x0C)) {
                Serial.printf("Signal detected %dms after HPD (SYS_STATUS=0x%02X)\n",
                              (i + 1) * 50, status);
                signalSeen = true;
                break;
            }
        }
        if (!signalSeen) {
            Serial.printf("Signal not detected within 5000ms — retrying HPD cycle...\n");
            writeReg8(HPD_CTL, 0x01);       // manual high
            delay(20);
            writeReg8(HPD_CTL, 0x00);       // low 100ms
            delay(100);
            writeReg8(HPD_CTL, 0x10);       // interlock
            delay(200);
            // HPD low clears EDID_MODE — re‑enable DDC
            writeReg8(EDID_MODE, MASK_EDID_MODE_DDC2B | MASK_EDID_MODE_E_DDC);
            for (int i = 0; i < 100; i++) {
                delay(50);
                uint8_t status = readReg8(SYS_STATUS);
                if ((status & 0x02) || ((status & 0x0C) == 0x0C)) {
                    Serial.printf("Signal detected %dms after HPD retry (SYS_STATUS=0x%02X)\n",
                                  (i + 1) * 50, status);
                    signalSeen = true;
                    break;
                }
            }
        }
        if (!signalSeen) {
            Serial.printf("Signal not detected within additional 5000ms\n");
        }
    }

    // ---- Final diagnostics ----
    {
        uint8_t st = readReg8(SYS_STATUS);
        uint8_t ddc = readReg8(DDC_CTL);
        uint8_t hpd = readReg8(HPD_CTL);
        uint8_t edidMode = readReg8(EDID_MODE);
        uint8_t hdmiCtl = readReg8(HDMI_CTL);
        uint8_t phyCtl1 = readReg8(PHY_CTL1);
        uint8_t phyCtl2 = readReg8(PHY_CTL2);
        uint8_t phyEn = readReg8(PHY_EN);
        uint8_t anaCtl = readReg8(ANA_CTL);
        uint8_t initEnd = readReg8(INIT_END);
        uint8_t phyBias = readReg8(PHY_BIAS);
        uint8_t phyCsq = readReg8(PHY_CSQ);
        uint8_t avmCtl = readReg8(AVM_CTL);
        uint8_t hdmiDet = readReg8(HDMI_DET);
        uint8_t hvRst = readReg8(HV_RST);
        uint16_t pllctl0 = readReg16(PLLCTL0);
        uint16_t pllctl1 = readReg16(PLLCTL1);
        uint16_t sysctl = readReg16(SYSCTL);
        uint8_t vi1 = readReg8(VI_STATUS1);
        uint8_t clk = readReg8(CLK_STATUS);
        uint8_t vi3 = readReg8(VI_STATUS3);
        uint8_t phyerr = readReg8(PHYERR_STATUS);
        // Measure TMDS clock frequency via FREQ_MON
        writeReg8(0x8690, 0x01);
        delay(5);
        uint16_t freqMon = readReg16(0x8692);
        uint32_t tmdsClkKhz = (uint32_t)freqMon * (_refclk_hz / 1000) / 65536;
        Serial.printf("Post-init: SYS_STATUS=0x%02X (DDC5V=%d TMDS=%d PLL=%d HDMI=%d SYNC=%d)\n",
                      st,
                      (st >> 0) & 1, (st >> 1) & 1, (st >> 2) & 1,
                      (st >> 4) & 1, (st >> 7) & 1);
        Serial.printf("           DDC(0x8543)=0x%02X HPD(0x8544)=0x%02X EDID_MODE(0x85C7)=0x%02X\n",
                      ddc, hpd, edidMode);
        Serial.printf("           HDMI_CTL=0x%02X PHY_CTL1=0x%02X PHY_CTL2=0x%02X PHY_EN=0x%02X\n",
                      hdmiCtl, phyCtl1, phyCtl2, phyEn);
        Serial.printf("           ANA_CTL=0x%02X INIT_END=0x%02X CLKM_CTL=0x%02X\n", anaCtl, initEnd, readReg8(CLKM_CTL));
        Serial.printf("           PHY_BIAS=0x%02X PHY_CSQ=0x%02X AVM_CTL=0x%02X HDMI_DET=0x%02X HV_RST=0x%02X\n",
                      phyBias, phyCsq, avmCtl, hdmiDet, hvRst);
        Serial.printf("           PLLCTL0=0x%04X PLLCTL1=0x%04X SYSCTL=0x%04X\n",
                      pllctl0, pllctl1, sysctl);
        Serial.printf("           VI1=0x%02X CLK=0x%02X VI3=0x%02X PHYERR=0x%02X\n",
                      vi1, clk, vi3, phyerr);
        Serial.printf("           CLK_STATUS: clk=%d dclk=%d stable=%d\n",
                      clk & 1, (clk >> 1) & 1, (clk >> 2) & 1);
        Serial.printf("           FREQ_MON=0x%04X TMDS_CLK=%u kHz\n",
                      freqMon, tmdsClkKhz);
    }

    probePD3400();

    return true;
}

// Probe DP-to-HDMI bridge at I2C address 0x18
static void probePD3400()
{
    Serial.printf("--- Bridge probe (I2C 0x18) ---\n");

    // Helper: read one byte with proper STOP between write and read
    auto readReg = [](uint8_t reg) -> int {
        Wire.beginTransmission(0x18);
        Wire.write(reg);
        if (Wire.endTransmission(true) != 0) return -1;  // STOP, not repeated START
        Wire.requestFrom(0x18, 1);
        if (!Wire.available()) return -1;
        return Wire.read();
    };

    // Registers 0x00-0x1F
    Serial.printf("  Regs 0x00-0x1F:\n ");
    for (uint8_t r = 0; r < 0x20; r++) {
        int v = readReg(r);
        if (v >= 0) {
            Serial.printf(" %02X:%02X", r, v);
            if ((r & 0x0F) == 0x0F) Serial.printf("\n");
        }
    }

    // Registers 0x60-0x6F (HDMI output) and 0x80-0xFF
    Serial.printf("  Regs 0x60-0x6F:\n ");
    for (uint8_t r = 0x60; r < 0x70; r++) {
        int v = readReg(r);
        if (v >= 0) {
            Serial.printf(" %02X:%02X", r, v);
            if ((r & 0x0F) == 0x0F) Serial.printf("\n");
        }
    }
    // Quick write test: write 0xAA to 0x00, read back, restore
    Wire.beginTransmission(0x18);
    Wire.write(0x00);
    Wire.write(0xAA);
    int wr = Wire.endTransmission(true);
    delay(5);
    int test = readReg(0x00);
    if (test == 0xAA) {
        Serial.printf("  -> is writable (GPIO expander or config chip)\n");
        // Restore
        Wire.beginTransmission(0x18);
        Wire.write(0x00);
        Wire.write(0x03);  // restore original value
        Wire.endTransmission(true);
    } else {
        Serial.printf("  -> read-only or write-ignored (test write returned 0x%02X)\n", test);
    }
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

    writeReg8(EDID_LEN1, len & 0xFF);
    writeReg8(EDID_LEN2, (len >> 8) & 0xFF);

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
    if (len == 0) return;
    // Combined I2C transaction: write register address, then read N bytes
    // (no stop between write and read — repeated start)
    _wire.beginTransmission(_addr);
    _wire.write((uint8_t)(reg >> 8));
    _wire.write((uint8_t)(reg & 0xFF));
    _wire.endTransmission(false);
    _wire.requestFrom((int)_addr, (int)len);
    for (size_t i = 0; i < len; i++) {
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

void TC358743::configureCsiTx() {
    writeReg16(CLW_CNTRL, 0x0000);
    writeReg16(D0W_CNTRL, 0x0000);
    writeReg16(D1W_CNTRL, 0x0000);
    writeReg16(D2W_CNTRL, 0x0000);
    writeReg16(D3W_CNTRL, 0x0000);

    writeReg32(HSTXVREGEN,
               MASK_CLM_HSTXVREGEN | MASK_D0M_HSTXVREGEN | MASK_D1M_HSTXVREGEN);

    writeReg32(CSI_CONFW, MASK_MODE_CLEAR | MASK_ADDRESS_CSI_CONTROL | 0);

    writeReg32(CSI_CONFW,
               MASK_MODE_SET | MASK_ADDRESS_CSI_CONTROL |
               MASK_CSI_MODE | MASK_TXHSMD | MASK_HSCKMD |
               MASK_NOL_2 | MASK_EOTDIS);

    writeReg32(CSI_CONFW,
               MASK_MODE_SET | MASK_ADDRESS_CSI_INT_ENA |
               MASK_IENHLT | MASK_IENER);

    writeReg32(TXOPTIONCNTRL, MASK_CONTCLKMODE);

    writeReg32(STARTCNTRL, MASK_START);

    Serial.println(F("CSI-2 TX: 2 lanes, continuous clock, started"));
}

void TC358743::configurePhy(uint8_t phyCtl0) {
    writeReg8(PHY_RST, 0x00);

    writeReg8(PHY_CTL0, phyCtl0);
    writeReg8(PHY_BIAS, 0x40);
    writeReg8(PHY_CSQ, 0x0a);
    writeReg8(PHY_CTL1, 0x80);
    writeReg8(PHY_CTL2, 0x00);

    delay(1);

    writeReg8(PHY_EN, 0x01);

    delay(3);

    writeReg8(PHY_RST, 0x01);

    delay(1);
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
