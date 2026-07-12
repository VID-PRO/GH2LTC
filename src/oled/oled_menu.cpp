#include "oled_menu.h"

#if OLED_ENABLE

OledMenu::OledMenu(OledDisplay &display)
    : _display(display), _count(0), _cursor(0), _scroll(0),
      _active(false), _lastActivity(0), _timeoutMs(DEFAULT_TIMEOUT_MS) {}

void OledMenu::clear() {
    _count = 0;
    _cursor = 0;
    _scroll = 0;
}

void OledMenu::addItem(const char* label, GetValueFn getValue,
                       ActionFn onOk, ActionFn onLongOk) {
    if (_count >= MENU_MAX_ITEMS) return;
    _items[_count].label = label;
    _items[_count].getValue = getValue;
    _items[_count].onOk = onOk;
    _items[_count].onLongOk = onLongOk;
    _count++;
}

void OledMenu::show() {
    _active = true;
    _cursor = 0;
    _scroll = 0;
    _lastActivity = millis();
}

void OledMenu::hide() {
    _active = false;
}

void OledMenu::up() {
    if (!_active || _count == 0) return;
    _lastActivity = millis();
    if (_cursor > 0) {
        _cursor--;
    } else {
        _cursor = _count - 1;
    }
    if (_cursor < (uint8_t)_scroll) _scroll = _cursor;
    if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
}

void OledMenu::down() {
    if (!_active || _count == 0) return;
    _lastActivity = millis();
    if (_cursor < _count - 1) {
        _cursor++;
    } else {
        _cursor = 0;
    }
    if (_cursor < (uint8_t)_scroll) _scroll = _cursor;
    if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
}

void OledMenu::ok(bool longPress) {
    if (!_active || _cursor >= _count) return;
    _lastActivity = millis();
    Item &item = _items[_cursor];
    if (longPress && item.onLongOk) {
        item.onLongOk();
    } else if (item.onOk) {
        item.onOk();
    }
}

void OledMenu::cancel() {
    if (!_active) return;
    hide();
}

void OledMenu::draw() {
    if (!_active) return;
    auto &u = _display.u8g2();

    u.firstPage();
    do {
        u.setFont(u8g2_font_6x10_tf);
        int w = u.getStrWidth(" SETTINGS ");
        u.drawStr((128 - w) / 2, 9, " SETTINGS ");

        for (uint8_t i = 0; i < VISIBLE; i++) {
            int idx = _scroll + i;
            if (idx >= _count) break;

            Item &item = _items[idx];
            int y = 21 + i * 11;

            if (idx == _cursor) {
                u.drawStr(2, y, ">");
            }

            char buf[22];
            const char *val = item.getValue ? item.getValue() : "";
            snprintf(buf, sizeof(buf), "%-13s%7s", item.label, val);

            int x = 12;
            if (u.getStrWidth(buf) > 128 - x) {
                int maxW = 128 - x - 1;
                int labelW = u.getStrWidth(item.label);
                if (labelW > maxW / 2) labelW = maxW / 2;
                int valW = u.getStrWidth(val);
                if (valW > maxW - labelW - 2) valW = maxW - labelW - 2;
                snprintf(buf, sizeof(buf), "%.*s %s", labelW / 6, item.label, val);
            }
            u.drawStr(x, y, buf);
        }

        if (_count > VISIBLE) {
            bool canScrollUp = _scroll > 0;
            bool canScrollDown = (_scroll + VISIBLE) < _count;
            if (canScrollUp && canScrollDown) {
                u.drawStr(120, 63, "^");
                u.drawStr(120, 55, "v");
            } else if (canScrollUp) {
                u.drawStr(124, 9, "^");
            } else if (canScrollDown) {
                u.drawStr(124, 9, "v");
            }
        }
    } while (u.nextPage());
}

bool OledMenu::tick() {
    if (!_active) return true;
    if (millis() - _lastActivity >= _timeoutMs) {
        hide();
        return false;
    }
    return true;
}

#endif
