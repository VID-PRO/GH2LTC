#pragma once
#include <Arduino.h>
#include <cstring>

class LtcDecoder {
public:
    using Callback = void (*)(uint8_t dd, uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff);

    LtcDecoder(uint8_t pin);
    void begin();
    void end();
    void setCallback(Callback cb) { _cb = cb; }
    void setFps(uint8_t fps);
    uint8_t fps() const { return _fps; }
    bool locked() const { return _locked; }
    void poll();

private:
    uint8_t _pin;
    Callback _cb = nullptr;
    uint8_t _fps = 25;
    uint64_t _halfBitUs = 0;
    bool _locked = false;
    uint64_t _lastSyncUs = 0;

    static const int BUF_SIZE = 256;
    volatile uint64_t _bufIntervals[BUF_SIZE];
    volatile int _bufHead = 0;
    volatile int _bufTail = 0;
    uint64_t _lastEdgeUs = 0;

    uint8_t _bits[128];
    int _bitCount = 0;
    enum { WAIT_EDGE, START_OF_CELL, MID_CELL_SEEN } _state = WAIT_EDGE;

    void decodeFrame(uint8_t *bits);
    void addBit(uint8_t bit);
    void resetSync();

    static void IRAM_ATTR isrHandler(void *arg);
    void IRAM_ATTR onEdge();
};
