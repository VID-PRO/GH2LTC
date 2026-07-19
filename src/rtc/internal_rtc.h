#pragma once
#include <Arduino.h>
#include <sys/time.h>

class InternalRtc {
public:
    InternalRtc() : _present(false) {}

    bool begin(int sda_pin, int scl_pin) {
        (void)sda_pin;
        (void)scl_pin;
        configTime(0, 0, nullptr);
        _present = true;
        return true;
    }

    bool isPresent() { return _present; }

    bool readTime(uint8_t &hh, uint8_t &mm, uint8_t &ss) {
        if (!_present) return false;
        time_t now;
        time(&now);
        struct tm *t = localtime(&now);
        if (!t) return false;
        hh = t->tm_hour;
        mm = t->tm_min;
        ss = t->tm_sec;
        return true;
    }

    bool setTime(uint8_t hh, uint8_t mm, uint8_t ss) {
        if (!_present) return false;
        time_t now;
        time(&now);
        struct tm t;
        localtime_r(&now, &t);
        t.tm_hour = hh;
        t.tm_min = mm;
        t.tm_sec = ss;
        time_t target = mktime(&t);
        struct timeval tv = { .tv_sec = target, .tv_usec = 0 };
        return settimeofday(&tv, nullptr) == 0;
    }

private:
    bool _present;
};
