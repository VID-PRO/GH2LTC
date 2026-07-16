#pragma once
#include <stdint.h>
#include "tc358743.h"
#include "tc358743_regs.h"

struct Gh5Timecode {
    uint8_t hh, mm, ss, ff;
};

inline uint8_t bcdToBin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

inline bool decodeGh5Timecode(TC358743 &tc, Gh5Timecode &out) {
    uint8_t buf[PK_ACP_LEN];
    tc.selectPacketType(PACKET_TYPE_VENDOR_SPECIFIC_IF);
    delay(2);
    tc.readBlock(PK_ACP_0HEAD, buf, sizeof(buf));
    if (buf[0] != 0x81) return false; // not a Vendor-Specific InfoFrame
    out.ff = bcdToBin(buf[10]);
    out.ss = bcdToBin(buf[11]);
    out.mm = bcdToBin(buf[12]);
    out.hh = bcdToBin(buf[13]);
    return true;
}
