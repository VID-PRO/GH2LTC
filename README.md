# [VID-PRO](https://www.vid-pro.de) GH2LTC

Reads Panasonic GH5 timecode from HDMI via TC358743 and regenerates it as SMPTE-12M LTC audio. Three PlatformIO environments: **master** (Waveshare ESP32-P4-WIFI6, HDMI receiver, BLE server), **slave** (ESP32-C3 Super Mini, BLE client, standalone LTC output), and **clap** (ESP32-C3, BLE client, LED matrix only).

---

## Features

| Category | Details |
|----------|---------|
| **HDMI timecode capture** | Reads GH5 InfoFrame metadata over I2C via TC358743 — no video decoding needed |
| **LTC generation** | Standalone SMPTE-12M biphase-mark encoder, esp_timer-driven, independent of I2C polling |
| **Frame rates** | Auto-detected from HDMI (24/25/30/50/60 fps) or manual via web UI |
| **RTC fallback** | Optional DS3231 preserves accurate time across power cycles with frame interpolation |
| **LED matrix** | 8 daisy-chained MAX7219 8×8 modules (64×8 px), software SPI; runtime toggle in web UI |
| **OLED display (optional)** | 128×64 SSD1306 on shared I2C bus shows timecode + HDMI lock status |
| **Web UI** | Fullscreen dark-teal SPA: timecode display, Auto/fixed FPS config, jam sync, brightness slider, matrix on/off, WiFi config |
| **WiFi** | AP on boot; auto-STA connect to saved network; AP re-enables on disconnect |
| **Reverse-engineer mode** | Dumps InfoFrame packets over serial to find GH5's exact timecode byte layout |
| **BLE wireless sync** | Master advertises timecode via BLE notify; slave/clap scan by service UUID, subscribe to notifications, run local LTC + display |

---

## Hardware

### Bill of Materials

#### Master (Waveshare ESP32-P4-WIFI6)
| Component | Notes | Buy |
|-----------|-------|-----|
| **Waveshare ESP32-P4-WIFI6** | ESP32-P4 + ESP32-C6 companion for WiFi/BLE | [waveshare.com](https://www.waveshare.com) |
| **TC358743 HDMI→CSI-2** | e.g. Geekworm C790 — I2C + CSI-2 | widely available |
| **22-pin to 15-pin CSI ribbon cable** | Connects TC358743 to ESP32-P4-WIFI6 CSI connector | search "22-pin to 15-pin CSI cable" |
| **MAX7219 8×8 LED matrix** | 8 daisy-chained modules (64×8 px) | widely available |
| **DS3231 RTC (optional)** | Battery-backed, I2C, ±2ppm | any electronics supplier |
| **128×64 OLED SSD1306 (optional)** | I2C, shares bus with TC358743 + RTC | any electronics supplier |
| **3.5mm TRS jack** | LTC audio output | any electronics supplier |
| **R1: 1kΩ, C1: 4.7nF, C2: 1µF** | LTC low-pass + DC block | — |
| **R3, R4: 10kΩ** | LTC level pad | — |
| **CR2032 coin cell** | Backup for DS3231 | any electronics supplier |

#### Slave (ESP32-C3 Super Mini)
| Component | Notes |
|-----------|-------|
| **ESP32-C3 Super Mini** | 400 MHz RISC-V, USB-C |
| **MAX7219 8×8 LED matrix** | 8 daisy-chained modules (64×8 px) |
| **DS3231 RTC (optional)** | I2C |
| **128×64 OLED SSD1306 (optional)** | I2C |
| **3.5mm TRS jack** | LTC audio output |
| **Passives** | Same RC filter as master |

### Pin Assignments

#### I2C Bus (shared — single peripheral)

| Signal | Master GPIO (CSI) | Slave GPIO | Device |
|--------|-------------------|------------|--------|
| SDA | 7 | 4 | TC358743 `0x0F` + OLED `0x3C` + DS3231 `0x68` |
| SCL | 8 | 5 | shared |

#### MAX7219 LED Matrix (software SPI)

| Signal | GPIO |
|--------|------|
| DIN | 2 |
| CS | 3 |
| CLK | 10 |

#### LTC Output

| Signal | GPIO |
|--------|------|
| LTC_OUT | 6 |

#### TC358743 Reset (not connected — rely on internal POR)

| Signal | GPIO |
|--------|------|
| TC_RESET | -1 (unused) |

---

## Software

### Required Libraries (`platformio.ini` `lib_deps`)

| Library | Version |
|---------|---------|
| `Wire` | built-in |
| `U8g2` | latest |
| `MD_MAX72XX` | latest |

### Environments

| Env | Board | Role | BLE | Platform |
|-----|-------|------|-----|----------|
| `slave` | ESP32-C3 DevKitC-02 | BLE client, LTC + OLED + RTC | ✓ (native C3) | `espressif32` |
| `clap` | ESP32-C3 Super Mini | BLE client, LED matrix only | ✓ (native C3) | `espressif32` |
| `master` | ESP32-P4-WIFI6 | HDMI receiver, BLE server | via C6 coprocessor† (ESP-Hosted SDIO) | `pioarduino/platform-espressif32` (GitHub) |

† ESP32-P4 has no native BLE controller. The Waveshare board's ESP32-C6 companion provides WiFi/BLE over SDIO via ESP-Hosted firmware (pre-flashed). All BLE code uses preprocessor guards (`SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE`) to compile correctly on P4.

### Building

```bash
# Slave (ESP32-C3, full I/O)
pio run -e slave -t upload

# Clap (ESP32-C3, LED matrix only)
pio run -e clap -t upload

# Master (ESP32-P4-WIFI6)
pio run -e master -t upload
```

Monitor:
```bash
pio device monitor -b 115200
```

### Project Layout

```
platformio.ini
src/config.h                   pin assignments, feature toggles (common base)
src/config_slave.h             -include for slave env
src/config_clap.h              -include for clap env
src/config_master.h            -include for master env
src/main.cpp                   compile-time dispatch: master/slave paths
src/webui/                     WiFi AP/STA, HTTP server, NVS, embedded JS/CSS/HTML
src/hdmi/                      TC358743 I2C driver + register map + GH5 timecode decoder
src/ltc/                       esp_timer-based SMPTE-12M LTC generator
src/matrix/                    MAX7219 64×8 framebuffer driver
src/oled/                      optional SSD1306 via U8g2
src/rtc/                       optional DS3231 RTC driver
src/timecode/                  BLE master (advertise/notify) & slave (scan/select/connect/subscribe)
```

---

## WiFi

| Scenario | Behavior |
|----------|----------|
| No WiFi configured | Opens AP with default SSID (master: `GH2LTC_XXXX`, slave: `TC-SLAVE-XXXX`, clap: `TC-CLAP-XXXX`), open network, `192.168.4.1` |
| Saved credentials exist | Connects as STA on boot; AP auto-disables on connect |
| STA disconnected >5 s | AP re-enabled for reconfiguration |

---

## Web Interface

Open `http://192.168.4.1` (AP mode) or the ESP's STA IP. The header displays a centered **VID-PRO** link (https://www.vid-prot.de).

| Feature | Description |
|---------|-------------|
| **Timecode display** | Fullscreen `dd:hh:mm:ss:ff` with color-coded status dot (green=HDMI, orange=RTC, gray=free) |
| **FPS config** | Auto (re-detect from HDMI) or fixed 24/25/30/50/60, drop-frame toggle, saved to NVS |
| **Jam sync** | Set timecode manually (dd:hh:mm:ss:ff) |
| **Brightness** | Slider for MAX7219 intensity (0–15), saved to NVS |
| **Matrix toggle** | Enable/disable LED matrix display at runtime, saved to NVS |
| **WiFi config** | SSID/password input, saved to NVS, forget option |
| **BLE (master)** | Change broadcast name, view connected slave count, disconnect all |
| **BLE (slave/clap)** | Scan for master devices (name + address), tap to connect, view connected master name, BLE connection indicator (bottom-left dots) on LED matrix |

---

## Configuration Defaults

| Setting | Master (HDMI + BLE server) | Slave (BLE client, no HDMI) | Clap (LED matrix only) |
|---------|---------------------------|-----------------------------|------------------------|
| WiFi AP SSID | `GH2LTC_` + last 4 MAC digits | `TC-SLAVE-` + last 4 MAC digits | `TC-CLAP-` + last 4 MAC digits |
| FPS | Auto (re-detect) | 25 | 25 |
| Drop frame | Off | Off | Off |
| RTC | Optional (DS3231) | Optional (DS3231) | Disabled |
| OLED | Optional (SSD1306) | Optional (SSD1306) | Disabled |
| MAX7219 matrix | Enabled by default | Enabled by default | Enabled by default |
| Matrix brightness | 4 (0–15) | 4 | 4 |
| BLE indicator (matrix) | — | — | 3-pixel dot (bottom-left) when connected |
| LTC output pin | GPIO6 | GPIO6 | GPIO6 |
| TC_RESET_PIN | -1 (unused) | — | — |
| Reverse-engineer mode | 0 | 0 | 0 |
| BLE role | Master (advertise + notify) | Slave (scan + subscribe) | Slave (scan + subscribe) |

---

## BLE Wireless Slave Sync

A custom 128-bit BLE service (`9a6f0001-...`) transfers timecode from master to slave as 5-byte notifications (dd, hh, mm, ss, ff):

- **Master**: advertises the service, sends a notification on every frame tick via `bleTimecodeUpdate()`. Broadcast name configurable via web UI; disconnect button removes all connected slaves.
- **Slave**: scans for devices offering the service (showing name + address), taps one to connect. On receiving a timecode packet it jams the local `LtcEncoder` in real time. The web UI displays the connected master's device name.

The slave/clap run their own independent LTC generator, MAX7219 matrix display (clap shows 3-pixel BLE indicator at bottom-left when connected), and web UI.

---

## Step 1: Reverse-engineer GH5 timecode bytes

Set `REVERSE_ENGINEER_MODE 1` in `src/config.h`. Connect the GH5 via HDMI, open serial monitor, set the GH5's timecode to a known value, then diff the InfoFrame dumps as time advances to find which bytes change. Fill in `decodeGh5Timecode()` in `src/hdmi/panasonic_tc.h`, then set `REVERSE_ENGINEER_MODE 0`.

When no HDMI source is detected (`TMDS=0`) the system free-runs, generating LTC from its internal timer starting at 01:00:00:00 (or RTC if fitted). It auto-switches between HDMI and free-run as sources are connected/disconnected.

---

## Time source fallback

1. **HDMI timecode** — directly drives LTC, syncs RTC once per second
2. **DS3231 RTC** — HH:MM:SS with sub-second frame interpolation via `millis()`
3. **Free-running tick** — advances from jam time (01:00:00:00)

---

## ⚠️ GH5 4K HDMI vs TC358743

The TC358743 only supports HDMI 1.4 up to 1080p. Set the GH5's "HDMI Rec Output" to FHD in the camera menu — this project only reads InfoFrame metadata, not pixel data.

## Troubleshooting TC358743

**CHIPID reads 0x0000** — the chip responds to I2C but doesn't identify as a genuine TC358743 (expected 0x44XX). Many breakout boards sold on AliExpress/Amazon are counterfeits or damaged. If you also see DDC5V toggling between 0→1 every 50ms and TMDS never goes high, the board is defective. No software change can fix this — replace the board.

**Test with a known-good HDMI source** (laptop, PC) before blaming the GH5. If both produce `TMDS=0`, the TC358743 breakout board is the problem, not the camera.

**WebUI JS crashes on clap** — if the config drawer shows `—` for BLE status and the name input stays empty, the browser may be loading a cached version of the page from before the JS `typeof` guard fix. Hard-refresh (Cmd+Shift+R) to load the updated script. The issue was a `ReferenceError` on `oledToggle`/`ltcToggle` (excluded by `#ifndef BLE_CLAP`) that propagated out of an IIFE and halted the entire `<script>` block before `pollBleSlave()` could register.

---

## Schematics

### LTC Output

```
GPIO6
  └───[ R1: 1k ]───┬───[ C2: 1µF ]───┬─── TRS TIP
                    │                 │
                 [ C1: 4.7nF ]    [ R3: 10k ]
                    │                 │
                   GND               └─── TRS RING (tie to TIP for mono)
                                     │
                                [ R4: 10k ]
                                     │
GND ─────────────────────────────────┴─── TRS SLEEVE
```

---

## Credits & References

- TC358743 register map: Linux kernel driver (`tc358743.c`)
- Geekworm C790: https://geekworm.com
- [MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX) LED matrix library
- LTC / SMPTE-12M: standard biphase-mark encoding
- [Waveshare ESP32-P4-WIFI6](https://www.waveshare.com/esp32-p4-wifi6.htm)
- [pioarduino/platform-espressif32](https://github.com/pioarduino/platform-espressif32) — community PlatformIO fork with ESP32-P4 support

---

## License

Provided as-is for personal and educational use. Panasonic and GH5 are trademarks of Panasonic Corporation.
