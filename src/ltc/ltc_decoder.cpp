#include "ltc_decoder.h"

LtcDecoder::LtcDecoder(uint8_t pin) : _pin(pin) {
    setFps(25);
}

void LtcDecoder::setFps(uint8_t fps) {
    if (fps < 1) fps = 25;
    _fps = fps;
    _halfBitUs = 1000000ULL / (80ULL * fps * 2ULL);
}

void LtcDecoder::begin() {
    pinMode(_pin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(_pin), isrHandler, this, CHANGE);
    _lastEdgeUs = 0;
    _state = WAIT_EDGE;
    _bitCount = 0;
    _bufHead = 0;
    _bufTail = 0;
    _locked = false;
}

void LtcDecoder::end() {
    detachInterrupt(digitalPinToInterrupt(_pin));
    _locked = false;
    _bufHead = 0;
    _bufTail = 0;
    _state = WAIT_EDGE;
    _bitCount = 0;
}

void IRAM_ATTR LtcDecoder::onEdge() {
    uint64_t now = esp_timer_get_time();
    if (_lastEdgeUs != 0) {
        uint64_t interval = now - _lastEdgeUs;
        if (interval < 5000) {
            int next = (_bufHead + 1) % BUF_SIZE;
            if (next != _bufTail) {
                _bufIntervals[next] = interval;
                _bufHead = next;
            }
        }
    }
    _lastEdgeUs = now;
}

void IRAM_ATTR LtcDecoder::isrHandler(void *arg) {
    ((LtcDecoder *)arg)->onEdge();
}

void LtcDecoder::resetSync() {
    _bitCount = 0;
    _state = WAIT_EDGE;
    _locked = false;
}

void LtcDecoder::addBit(uint8_t bit) {
    if (_bitCount >= 128) {
        memmove(_bits, _bits + 16, _bitCount - 16);
        _bitCount -= 16;
    }
    _bits[_bitCount++] = bit;
    if (_bitCount >= 16) {
        static const uint8_t syncWord[16] = {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1};
        int start = _bitCount - 16;
        bool found = true;
        for (int i = 0; i < 16; i++) {
            if (_bits[start + i] != syncWord[i]) { found = false; break; }
        }
        if (found && start >= 64) {
            decodeFrame(_bits + (start - 64));
            if (_lastSyncUs != 0) {
                uint64_t now = esp_timer_get_time();
                uint64_t period = now - _lastSyncUs;
                uint8_t detectedFps = 1000000 / period;
                if (detectedFps >= 20 && detectedFps <= 100 && detectedFps != _fps) {
                    setFps(detectedFps);
                }
            }
            _lastSyncUs = esp_timer_get_time();
            _locked = true;
            _bitCount = 0;
        }
    }
}

void LtcDecoder::decodeFrame(uint8_t *bits) {
    auto bcdVal = [&](int start, int count) -> uint8_t {
        uint8_t v = 0;
        for (int i = 0; i < count; i++) {
            if (bits[start + i]) v |= (1 << i);
        }
        return v;
    };

    uint8_t ff = bcdVal(0, 4) + bcdVal(8, 2) * 10;
    uint8_t ss = bcdVal(16, 4) + bcdVal(24, 3) * 10;
    uint8_t mm = bcdVal(32, 4) + bcdVal(40, 3) * 10;
    uint8_t hh = bcdVal(48, 4) + bcdVal(56, 2) * 10;

    if (ff >= 60 || ss >= 60 || mm >= 60 || hh >= 24) return;

    if (_cb) _cb(0, hh, mm, ss, ff);
}

void LtcDecoder::poll() {
    while (_bufTail != _bufHead) {
        int idx = (_bufTail + 1) % BUF_SIZE;
        uint64_t interval = _bufIntervals[idx];
        _bufTail = idx;

        if (_halfBitUs == 0) continue;

        uint64_t tol = _halfBitUs / 3;
        if (tol < 30) tol = 30;

        bool isShort = (interval >= _halfBitUs - tol && interval <= _halfBitUs + tol);
        bool isLong  = (interval >= 2 * _halfBitUs - tol && interval <= 2 * _halfBitUs + tol);

        if (_state == WAIT_EDGE) {
            if (isShort || isLong) _state = START_OF_CELL;
            continue;
        }

        if (isShort) {
            if (_state == START_OF_CELL) {
                _state = MID_CELL_SEEN;
            } else if (_state == MID_CELL_SEEN) {
                addBit(1);
                _state = START_OF_CELL;
            }
        } else if (isLong) {
            if (_state == START_OF_CELL) {
                addBit(0);
            } else {
                resetSync();
            }
        } else {
            resetSync();
        }
    }
}
