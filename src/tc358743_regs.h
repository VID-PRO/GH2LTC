#pragma once
// Subset of TC358743 register map, taken from the Linux kernel driver
// (drivers/media/i2c/tc358743_regs.h, GPL-2.0). Only the registers this
// project needs are reproduced here.

#define CHIPID                0x0000
#define FIFOCTL               0x0006

// SYSCTL — system control (16-bit), address 0x0002
#define SYSCTL                0x0002
#define MASK_SLEEP            0x8000
#define MASK_CTXRST           0x1000
#define MASK_HDMIRST          0x0100
#define MASK_CECRST           0x0002
#define MASK_IRRST            0x0001

#define SYS_INT               0x8502
#define SYS_INTM              0x8512
#define SYS_STATUS            0x8520   // bit0=DDC5V, bit1=TMDSclk, bit2=PLLlock, bit3=PHYsdt, bit4=HDMI, bit5=HDCP
#define VI_STATUS1            0x8522
#define VI_STATUS3            0x8528

#define PHY_CTL0              0x8531
#define PHY_CTL1              0x8532
#define PHY_CTL2              0x8533
#define PHY_EN                0x8534
#define PHY_RST               0x8535
#define PHY_BIAS              0x8536
#define PHY_CSQ               0x853F

#define SYS_FREQ0                             0x8540
#define SYS_FREQ1                             0x8541

#define SYS_CLK                               0x8542
#define MASK_CLK_DIFF                         0x0C
#define MASK_CLK_DIV                          0x03
#define CLKM_CTL              0x8562   // bit0: PLL_REF_FREQ — 0=27MHz, 1=26/42MHz
#define DDC_CTL               0x8543
#define HPD_CTL               0x8544
#define ANA_CTL               0x8545
#define AVM_CTL               0x8546
#define INIT_END              0x854A
#define HDMI_DET              0x8552

#define FH_MIN0               0x85AA
#define FH_MIN1               0x85AB
#define FH_MAX0               0x85AC
#define FH_MAX1               0x85AD
#define HV_RST                0x85AF

#define LOCKDET_REF0          0x8630
#define LOCKDET_REF1          0x8631
#define LOCKDET_REF2          0x8632

#define NCO_F0_MOD            0x8670

#define EDID_MODE             0x85C7
#define MASK_EDID_MODE_DISABLE  0x00
#define MASK_EDID_MODE_DDC2B    0x01
#define MASK_EDID_MODE_E_DDC    0x02
#define EDID_SEG_NUM          0x85C8
#define EDID_SEG              0x85C9
#define EDID_LEN1             0x85CA
#define EDID_LEN2             0x85CB
#define EDID_RAM              0x8C00

// ===== MIPI D-PHY lane control (16-bit) =====
#define CLW_CNTRL             0x0140
#define D0W_CNTRL             0x0144
#define D1W_CNTRL             0x0148
#define D2W_CNTRL             0x014C
#define D3W_CNTRL             0x0150

// ===== CSI-2 TX control (32-bit) =====
#define TXOPTIONCNTRL         0x0238
#define MASK_CONTCLKMODE      0x00000001

#define CSI_CONTROL           0x040C
#define MASK_CSI_MODE         0x8000
#define MASK_HTXTOEN          0x0400
#define MASK_TXHSMD           0x0080
#define MASK_HSCKMD           0x0020
#define MASK_NOL              0x0006
#define MASK_NOL_1            0x0000
#define MASK_NOL_2            0x0002
#define MASK_NOL_3            0x0004
#define MASK_NOL_4            0x0006
#define MASK_EOTDIS           0x0001

#define CSI_INT               0x0414
#define CSI_INT_ENA           0x0418
#define CSI_ERR               0x044C
#define CSI_CONFW             0x0500
#define MASK_MODE_SET         0xA0000000
#define MASK_MODE_CLEAR       0xC0000000
#define MASK_ADDRESS_CSI_CONTROL   0x03000000
#define MASK_ADDRESS_CSI_INT_ENA   0x06000000
#define MASK_ADDRESS_CSI_ERR_HALT  0x15000000
#define MASK_IENHLT           0x00000008
#define MASK_IENER            0x00000004

#define CSI_START             0x0518
#define MASK_STRT             0x00000001
#define STARTCNTRL            0x0204
#define MASK_START            0x00000001

// ===== D-PHY timing (32-bit each, use defaults if unsure) =====
#define LINEINITCNT           0x0210
#define LPTXTIMECNT           0x0214
#define TCLK_HEADERCNT        0x0218
#define TCLK_TRAILCNT         0x021C
#define THS_HEADERCNT         0x0220
#define TWAKEUP               0x0224
#define TCLK_POSTCNT          0x0228
#define THS_TRAILCNT          0x022C
#define HSTXVREGCNT           0x0230

#define HSTXVREGEN            0x0234
#define MASK_CLM_HSTXVREGEN   0x0001
#define MASK_D0M_HSTXVREGEN   0x0002
#define MASK_D1M_HSTXVREGEN   0x0004
#define MASK_D2M_HSTXVREGEN   0x0008
#define MASK_D3M_HSTXVREGEN   0x0010

#define FORCE_MUTE            0x8600
#define HDCP_REG1             0x8563
#define HDCP_REG2             0x8564

#define VI_MODE               0x8570
#define VOUT_SET2             0x8573
#define VOUT_SET3             0x8574
#define VI_REP                0x8576
#define VI_MUTE               0x857F
#define MASK_AUTO_MUTE        0xC0
#define MASK_VI_MUTE          0x10

// --- Packet-type slot select & the rotating ACP/InfoFrame buffer ---
#define TYP_ACP_SET           0x8706

// --- InfoFrame buffers (each holds: HEAD(3 bytes incl. length) + payload) ---
#define PK_AVI_0HEAD          0x8710
#define PK_AVI_16BYTE         0x8720
#define PK_AVI_LEN            (PK_AVI_16BYTE - PK_AVI_0HEAD + 1)

#define PK_VS_0HEAD           0x8770   // Vendor-Specific InfoFrame - first place to look for embedded TC
#define PK_VS_27BYTE          0x878E
#define PK_VS_LEN             (PK_VS_27BYTE - PK_VS_0HEAD + 1)

#define PK_ACP_0HEAD          0x8790   // rotating slot, type selected via TYP_ACP_SET
#define PK_ACP_27BYTE         0x87AE
#define PK_ACP_LEN            (PK_ACP_27BYTE - PK_ACP_0HEAD + 1)

#define PK_AUD_0HEAD          0x8730
#define PK_AUD_LEN            10

#define PK_SPD_0HEAD          0x8740
#define PK_SPD_LEN            29

#define PK_MS_0HEAD           0x8750
#define PK_MS_LEN             10

#define PK_ISRC1_0HEAD        0x8760
#define PK_ISRC1_LEN          18

#define PK_ISRC2_0HEAD        0x8768
#define PK_ISRC2_LEN          18

// Programmable packet type IDs (write to TYP_ACP_SET to select what shows up
// in the PK_ACP_0HEAD buffer). Per CEA-861 packet type table.
#define PACKET_TYPE_ACP                0x04
#define PACKET_TYPE_ISRC1              0x05
#define PACKET_TYPE_ISRC2              0x06
#define PACKET_TYPE_GAMUT_METADATA     0x0A
#define PACKET_TYPE_VENDOR_SPECIFIC_IF 0x81
#define PACKET_TYPE_AVI_IF             0x82
#define PACKET_TYPE_SPD_IF             0x83
#define PACKET_TYPE_AUDIO_IF           0x84
#define PACKET_TYPE_MPEG_SOURCE_IF     0x85
#define PACKET_TYPE_DRM_IF             0x87
