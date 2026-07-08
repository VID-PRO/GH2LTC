#pragma once
// Subset of TC358743 register map, taken from the Linux kernel driver
// (drivers/media/i2c/tc358743_regs.h, GPL-2.0). Only the registers this
// project needs are reproduced here.

#define CHIPID                0x0000

#define SYS_INT               0x8502
#define SYS_INTM              0x8512
#define SYS_STATUS            0x8520   // bit0 = TMDS clock detected, bit1 = HDMI mode (vs DVI)
#define VI_STATUS1            0x8522
#define VI_STATUS3            0x8528

#define PHY_CTL0              0x8531
#define PHY_CTL1              0x8532
#define PHY_CTL2              0x8533
#define PHY_EN                0x8534
#define PHY_RST               0x8535
#define PHY_BIAS              0x8536
#define PHY_CSQ               0x853F

#define SYS_FREQ0             0x8540
#define SYS_FREQ1             0x8541
#define DDC_CTL               0x8543
#define HPD_CTL               0x8544
#define ANA_CTL               0x8545
#define AVM_CTL               0x8546
#define INIT_END              0x854A
#define HDMI_DET              0x8552

#define EDID_MODE             0x85C7
#define EDID_LEN1             0x85CA
#define EDID_LEN2             0x85CB

#define FORCE_MUTE            0x8600
#define CSI_LANE_ENABLE       0x8601   // CSI lanes - unused by us but part of std init
#define HDCP_REG1             0x8563
#define HDCP_REG2             0x8564

#define VI_MODE               0x8570
#define VOUT_SET2             0x8573
#define VOUT_SET3             0x8574
#define VI_REP                0x8576
#define VI_MUTE               0x857F

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
