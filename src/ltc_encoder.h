#pragma once
#include <Arduino.h>

// Self-contained SMPTE 12M Linear Timecode (LTC) generator.
//
// LTC is an 80-bit frame, biphase-mark coded, transmitted as audio at
// (80 * fps) bits/sec. We precompute the 80 data bits for "now" whenever the
// caller calls setTime(), and an Arduino hw_timer ISR continuously walks the
// biphase-mark state machine to toggle the output GPIO — this runs
// completely independently of the main loop / I2C polling, so LTC output
// stays smooth even while we're busy reading the HDMI receiver.
class LtcEncoder {
public:
    // pin: GPIO driving the (external RC filter ->) audio output.
    // fps: integer frame rate (24, 25, 30 ...). For 29.97 use fps=30 with
    //      dropFrame=true.
    LtcEncoder(uint8_t pin, uint8_t fps, bool dropFrame = false);

    void begin();

    // Call this once per incoming video frame with the current timecode.
    void setTime(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff);

    void setDd(uint8_t dd) { _dd = dd; }

    // Advance internal LTC time by one frame and keep generating.
    void tick();

    // Enable/disable LTC audio output (stops/starts the ISR timer)
    void setEnabled(bool en);
    bool enabled() const { return _enabled; }

    // Returns true once (and clears the flag) the first time this is called
    // after a full 80-bit LTC frame has finished transmitting on the wire.
    uint16_t framesCompleted();

    // Dynamically change the frame rate (e.g. after auto-detection).
    void setFps(uint8_t fps, bool dropFrame);

    uint8_t fps() const { return _fps; }
    bool dropFrame() const { return _dropFrame; }
    uint8_t dd() const { return _dd; }
    uint8_t hh() const { return _hh; }
    uint8_t mm() const { return _mm; }
    uint8_t ss() const { return _ss; }
    uint8_t ff() const { return _ff; }

private:
    uint8_t _pin;
    uint8_t _fps;
    bool _dropFrame;
    uint64_t _halfBitPeriodUs;

    volatile bool _enabled = true;
    volatile uint8_t _bits[80];
    volatile uint16_t _framesCompleted = 0;

    uint8_t _hh = 0, _mm = 0, _ss = 0, _ff = 0, _dd = 0;

    hw_timer_t * _timer = nullptr;

    void encodeFrame();
    void IRAM_ATTR onHalfBitTick();
    void startTimer();
    void stopTimer();

    static void IRAM_ATTR isrTrampoline();
    static LtcEncoder *_instance;
};
