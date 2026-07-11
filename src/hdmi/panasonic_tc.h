#pragma once
#include <stdint.h>
#include "tc358743.h"

// ---------------------------------------------------------------------------
// Panasonic GH5 embedded-timecode decoder - PLACEHOLDER.
//
// Panasonic does not publish which HDMI packet/byte offset carries the GH5's
// internal timecode, so this needs a short one-time reverse-engineering pass
// (see README.md "Reverse-engineering the timecode bytes"). Use
// REVERSE_ENGINEER_MODE=1 to dump candidate buffers over Serial, change the
// GH5's displayed TC to a known value, and find which bytes track it.
//
// Once found, fill in decodeGh5Timecode() below: read the relevant buffer via
// tc.readBlock(...), pull out HH/MM/SS/FF, and return true. Until then this
// returns false and main.cpp will fall back to free-running LTC from a
// manually jammed start value (same as the "Option A" approach), so the
// firmware is still useful as a standalone LTC generator even before the
// HDMI decode step is finished.
// ---------------------------------------------------------------------------

struct Gh5Timecode {
    uint8_t hh, mm, ss, ff;
};

inline bool decodeGh5Timecode(TC358743 &tc, Gh5Timecode &out) {
    // --- EXAMPLE STRUCTURE (not yet confirmed against real hardware) ---
    // uint8_t buf[PK_VS_LEN];
    // tc.readBlock(PK_VS_0HEAD, buf, sizeof(buf));
    // if (buf[0] != 0x81) return false; // not a Vendor-Specific InfoFrame
    // out.ff = bcdToBin(buf[X]);
    // out.ss = bcdToBin(buf[X+1]);
    // out.mm = bcdToBin(buf[X+2]);
    // out.hh = bcdToBin(buf[X+3]);
    // return true;

    (void)tc;
    out = {0, 0, 0, 0};
    return false; // TODO: implement once the byte layout is found
}

inline uint8_t bcdToBin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}
