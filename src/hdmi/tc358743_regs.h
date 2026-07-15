#pragma once
// Subset of TC358743 register map, taken from the Linux kernel driver
// (drivers/media/i2c/tc358743_regs.h, GPL-2.0). Only the registers this
// project needs are reproduced here.

#define CHIPID                0x0000
#define FIFOCTL               0x0006

// SYSCTL — system control (16-bit), address 0x0002
#define SYSCTL                0x0002
#define MASK_SLEEP            0x0001
#define MASK_CTXRST           0x0200
#define MASK_HDMIRST          0x0100
#define MASK_CECRST           0x0400
#define MASK_IRRST            0x0800

// PLL + CSI core control (p4kvm)
#define CONFCTL               0x0004
#define MASK_VBUFEN           0x0001
#define MASK_ABUFEN           0x0002
#define PLLCTL0               0x0020      // CSI-TX PLL
#define PLLCTL1               0x0022      // CSI-TX PLL
#define MASK_CKEN             0x0010
#define MASK_RESETB           0x0002
#define MASK_PLL_EN           0x0001
#define MASK_PLL_FRS          0x0C00
#define INTSTATUS             0x0014
#define INTMASK               0x0016
#define MASK_HDMI_MSK         0x0200
#define MASK_CSI_MSK          0x0100
#define MASK_INTER            0x00000004
#define MASK_TXBRK            0x00000002
#define MASK_QUNK             0x00000010
#define MASK_WCER             0x00000100
#define MASK_INER             0x00000200
#define MASK_ADDRESS_CSI_ERR_INTENA  0x14000000

#define SYS_INT               0x8502
#define SYS_INTM              0x8512
#define SYS_STATUS            0x8520   // bit0=DDC5V, bit1=TMDSclk, bit2=PLLlock, bit3=PHYsdt, bit4=HDMI, bit5=HDCP
#define VI_STATUS1            0x8522
#define VI_STATUS2            0x8525
#define CLK_STATUS            0x8526
#define PHYERR_STATUS         0x8527
#define VI_STATUS3            0x8528

#define VI_HTOTAL             0x8506
#define VI_VTOTAL             0x8508
#define VI_HACTIVE            0x850A
#define VI_VACTIVE            0x850C
#define VI_HFPORCH            0x850E
#define VI_HSW                0x850F
#define VI_HBPORCH            0x8510
#define VI_VFPORCH            0x8511
#define VI_VSW                0x8512
#define VI_VBPORCH            0x8513

#define PHY_CTL0              0x8531
#define PHY_CTL1              0x8532
#define PHY_CTL2              0x8533
#define PHY_EN                0x8534
#define PHY_RST               0x8535
#define PHY_BIAS              0x8536
#define PHY_CSQ               0x853F
#define PHY_FREQ              0x0812
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
#define HDMI_CTL              0x8550
#define MASK_HDMI_CTL_HDMI    0x10
#define HDMI_DET              0x8552
#define MASK_HDMI_DET_V       0x30

#define FH_MIN0               0x85AA
#define FH_MIN1               0x85AB
#define FH_MAX0               0x85AC
#define FH_MAX1               0x85AD
#define HV_RST                0x85AF

#define LOCKDET_REF0          0x8630
#define LOCKDET_REF1          0x8631
#define LOCKDET_REF2          0x8632

#define NCO_F0_MOD            0x8670

// TMDS clock frequency measurement: write 0x01 to start, wait 5ms, read 16-bit result
#define FREQ_MON_CTL          0x8690
#define FREQ_MON_DATA         0x8692

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

#define HDCP_MODE             0x8560
#define MASK_HDCP_MANUAL_AUTH 0x02
#define HDCP_REG1             0x8563
#define HDCP_REG2             0x8564

// Audio init registers (p4kvm set_hdmi_audio)
#define FORCE_MUTE            0x8602
#define AUTO_CMD0             0x8603
#define AUTO_CMD1             0x8604
#define AUTO_CMD2             0x8605
#define BUFINIT_START         0x8606
#define FS_MUTE               0x8607
#define FS_IMODE              0x8608
#define ACR_MODE              0x8609
#define ACR_MDF0              0x860A
#define ACR_MDF1              0x860B
#define SDO_MODE1             0x860D
#define DIV_MODE              0x8612

#define VI_MODE               0x8570
#define VOUT_SET2             0x8573
#define VOUT_SET3             0x8574
#define MASK_VOUT_EXTCNT      0x08
#define VI_REP                0x8576
#define VI_MUTE               0x857F
#define MASK_AUTO_MUTE        0xC0
#define MASK_VI_MUTE          0x10

// InfoFrame / packet-limit registers (p4kvm set_hdmi_info_frame)
#define PK_INT_MODE           0x8709
#define NO_PKT_LIMIT          0x870B
#define NO_PKT_CLR            0x870C
#define ERR_PK_LIMIT          0x870D
#define NO_PKT_LIMIT2         0x870E
#define NO_GDB_LIMIT          0x9007

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
