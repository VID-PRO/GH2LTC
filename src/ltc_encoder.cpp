#include "ltc_encoder.h"
#include <esp_timer.h>

LtcEncoder *LtcEncoder::_instance = nullptr;

namespace {
    volatile bool gpioState = false;
    volatile uint8_t halfBitIndex = 0;
}

LtcEncoder::LtcEncoder(uint8_t pin, uint8_t fps, bool dropFrame)
    : _pin(pin), _fps(fps), _dropFrame(dropFrame), _halfBitPeriodUs(0), _timer(nullptr) {
    for (int i = 0; i < 80; i++) _bits[i] = 0;
}

void LtcEncoder::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    _instance = this;
    encodeFrame();
    startTimer();
}

void LtcEncoder::startTimer() {
    double halfBitPeriodUs = 1000000.0 / (80.0 * _fps * 2.0);
    _halfBitPeriodUs = (uint64_t)(halfBitPeriodUs + 0.5);

    esp_timer_create_args_t args = {};
    args.callback = &isrTrampoline;
    args.name = "ltc_timer";
    esp_timer_create(&args, (esp_timer_handle_t *)&_timer);
    esp_timer_start_periodic((esp_timer_handle_t)_timer, _halfBitPeriodUs);
}

void LtcEncoder::stopTimer() {
    if (_timer) {
        esp_timer_stop((esp_timer_handle_t)_timer);
        esp_timer_delete((esp_timer_handle_t)_timer);
        _timer = nullptr;
    }
}

void LtcEncoder::setFps(uint8_t fps, bool dropFrame) {
    _fps = fps;
    _dropFrame = dropFrame;
    double halfBitPeriodUs = 1000000.0 / (80.0 * _fps * 2.0);
    _halfBitPeriodUs = (uint64_t)(halfBitPeriodUs + 0.5);
    if (_timer) {
        esp_timer_stop((esp_timer_handle_t)_timer);
        esp_timer_start_periodic((esp_timer_handle_t)_timer, _halfBitPeriodUs);
    }
}

void LtcEncoder::setTime(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff) {
    _hh = hh % 24;
    _mm = mm % 60;
    _ss = ss % 60;
    _ff = ff % _fps;
    encodeFrame();
}

void LtcEncoder::tick() {
    _ff++;
    if (_ff >= _fps) {
        _ff = 0;
        _ss++;
        if (_ss >= 60) {
            _ss = 0;
            _mm++;
            if (_mm >= 60) {
                _mm = 0;
                _hh = (_hh + 1) % 24;
                if (_hh == 0) _dd++;
            }
        }
    }
    encodeFrame();
}

static inline void toBcd(uint8_t value, uint8_t &units, uint8_t &tens) {
    units = value % 10;
    tens = value / 10;
}

void LtcEncoder::encodeFrame() {
    uint8_t bits[80];
    for (int i = 0; i < 80; i++) bits[i] = 0;

    uint8_t ffU, ffT, ssU, ssT, mmU, mmT, hhU, hhT;
    toBcd(_ff, ffU, ffT);
    toBcd(_ss, ssU, ssT);
    toBcd(_mm, mmU, mmT);
    toBcd(_hh, hhU, hhT);

    auto setNibble = [&](int startBit, uint8_t value, int numBits) {
        for (int b = 0; b < numBits; b++) {
            bits[startBit + b] = (value >> b) & 0x01;
        }
    };

    setNibble(0, ffU, 4);
    setNibble(8, ffT, 2);
    bits[10] = _dropFrame ? 1 : 0;
    bits[11] = 0;
    setNibble(16, ssU, 4);
    setNibble(24, ssT, 3);
    bits[27] = 0;
    setNibble(32, mmU, 4);
    setNibble(40, mmT, 3);
    bits[43] = 0;
    setNibble(48, hhU, 4);
    setNibble(56, hhT, 2);
    bits[58] = 0;

    static const uint8_t syncWord[16] = {
        0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1
    };
    for (int i = 0; i < 16; i++) bits[64 + i] = syncWord[i];

    int onesCount = 0;
    for (int i = 0; i < 80; i++) if (i != 59) onesCount += bits[i];
    bits[59] = (onesCount % 2 == 0) ? 0 : 1;

    noInterrupts();
    for (int i = 0; i < 80; i++) _bits[i] = bits[i];
    interrupts();
}

void IRAM_ATTR LtcEncoder::onHalfBitTick() {
    uint8_t bitIndex = halfBitIndex / 2;
    bool isStartOfBitCell = (halfBitIndex % 2) == 0;

    if (isStartOfBitCell) {
        gpioState = !gpioState;
        digitalWrite(_pin, gpioState);
    } else {
        if (_bits[bitIndex]) {
            gpioState = !gpioState;
            digitalWrite(_pin, gpioState);
        }
    }

    halfBitIndex++;
    if (halfBitIndex >= 160) {
        halfBitIndex = 0;
        _framesCompleted++;
    }
}

void IRAM_ATTR LtcEncoder::isrTrampoline(void *) {
    if (_instance) _instance->onHalfBitTick();
}

uint16_t LtcEncoder::framesCompleted() {
    noInterrupts();
    uint16_t v = _framesCompleted;
    _framesCompleted = 0;
    interrupts();
    return v;
}
