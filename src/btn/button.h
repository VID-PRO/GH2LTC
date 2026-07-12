#pragma once
#include <Arduino.h>

class Button {
public:
    Button(uint8_t pin);
    void begin(bool pullup = true);
    void read();

    bool pressed()  const { return _pressed; }
    bool released() const { return _released; }
    bool held()     const { return _state;   }
    bool heldFor(unsigned long ms) const;

private:
    uint8_t _pin;
    bool _state;
    bool _lastRaw;
    bool _pressed;
    bool _released;
    unsigned long _changeTime;
    static const unsigned long DEBOUNCE_MS = 30;
};
