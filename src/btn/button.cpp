#include "button.h"

Button::Button(uint8_t pin)
    : _pin(pin), _state(false), _lastRaw(false),
      _pressed(false), _released(false), _changeTime(0) {}

void Button::begin(bool pullup) {
    pinMode(_pin, pullup ? INPUT_PULLUP : INPUT);
    _state = false;
    _lastRaw = false;
    _pressed = false;
    _released = false;
    _changeTime = 0;
}

void Button::read() {
    bool raw = (digitalRead(_pin) == LOW);
    if (raw != _lastRaw) {
        _lastRaw = raw;
        _changeTime = millis();
    }

    unsigned long now = millis();
    if (now - _changeTime >= DEBOUNCE_MS) {
        if (raw != _state) {
            _state = raw;
            if (_state) {
                _pressed = true;
            } else {
                _released = true;
            }
        }
    }
}

bool Button::heldFor(unsigned long ms) const {
    if (!_state) return false;
    if (millis() - _changeTime < ms) return false;
    return true;
}
