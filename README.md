# [VID-PRO](https://www.vid-prot.de) GH5-LTC-ESP

Reads Panasonic GH5 timecode from HDMI via TC358743 and regenerates it as SMPTE-12M LTC audio on an ESP32-C3 Super Mini with an 8×8 LED matrix display, full web UI, and **BLE wireless slave sync**.

---

## Features

| Category | Details |
|----------|---------|
| **HDMI timecode capture** | Reads GH5 InfoFrame metadata over I2C via TC358743 — no video decoding needed |
| **LTC generation** | Standalone SMPTE-12M biphase-mark encoder, esp_timer-driven, independent of I2C polling |
| **Frame rates** | Auto-detected from HDMI (24/25/30/50/60 fps) or manual via web UI |
| **RTC fallback** | Optional DS3231 preserves accurate time across power cycles with frame interpolation |
| **LED matrix** | 8 daisy-chained MAX7219 8×8 modules (64×8 px), dd:hh:mm:ss.ff format, software SPI; runtime toggle in web UI |
| **OLED display (optional)** | 128×64 SSD1306 on shared I2C bus shows timecode + HDMI lock status |
| **Web UI** | Fullscreen dark-teal SPA: timecode display, Auto/fixed FPS config, jam sync, brightness slider, matrix on/off, WiFi config |
| **WiFi** | AP `TC-LTC-GENERATOR` on boot; auto-STA connect to saved network; AP re-enables on disconnect |
| **Reverse-engineer mode** | Dumps InfoFrame packets over serial to find GH5's exact timecode byte layout |
| **BLE wireless sync** | Master advertises timecode via BLE notify; slave scans by service UUID, selects a master, subscribes to notifications, and runs local LTC + displays. Slave web UI shows the connected master's device name. Master name and disconnect configurable via web UI. |

---

## Hardware

### Bill of Materials

| Component | Notes | Buy |
|-----------|-------|-----|
| **ESP32-C3 Super Mini** | 400 MHz RISC-V, USB-C | widely available |
| **TC358743 HDMI→CSI-2** | e.g. Geekworm C790 — I2C + power only | https://geekworm.com |
| **MAX7219 8×8 LED matrix** | 8 daisy-chained modules (64×8 px) | widely available |
| **DS3231 RTC (optional)** | Battery-backed, I2C, ±2ppm | any electronics supplier |
| **128×64 OLED SSD1306 (optional)** | I2C, shares bus with TC358743 + RTC | any electronics supplier |
| **15-pin FPC→DIP breakout** | For C790 ribbon cable | search "15-pin 1.0mm FPC to DIP breakout" |
| **3.5mm TRS jack** | LTC audio output | any electronics supplier |
| **R1: 1kΩ, C1: 4.7nF, C2: 1µF** | LTC low-pass + DC block | — |
| **R3, R4: 10kΩ** | LTC level pad | — |
| **CR2032 coin cell** | Backup for DS3231 | any electronics supplier |

### Pin Assignments

**I2C Bus (shared — ESP32-C3 has one peripheral)**

| Signal | GPIO | Device |
|--------|------|--------|
| SDA | 4 | TC358743 `0x0F` + OLED `0x3C` + DS3231 `0x68` |
| SCL | 5 | shared |

**MAX7219 LED Matrix (software SPI)**

| Signal | GPIO |
|--------|------|
| DIN | 2 |
| CS | 3 |
| CLK | 10 |

**LTC Output**

| Signal | GPIO |
|--------|------|
| LTC_OUT | 6 |

**Status LED**

| Signal | GPIO |
|--------|------|
| STATUS_LED | 8 |

---

## Software

### Required Libraries (`platformio.ini` `lib_deps`)

| Library | Version |
|---------|---------|
| `Wire` | built-in |
| `U8g2` | latest |
| `MD_MAX72XX` | latest |

### Board Settings

| Setting | Value |
|---------|-------|
| Platform | `espressif32` |
| Board | `esp32-c3-devkitc-02` |
| Framework | `arduino` |
| Core | 2.0.17 (ESP-IDF v4.4.7) |
| CPU | 160 MHz |
| USB CDC | Enabled (`-DARDUINO_USB_CDC_ON_BOOT=1`) |
| Monitor Speed | 115200 |

### Building

1. Install [PlatformIO CLI](https://platformio.org/install/cli) (or VS Code extension).
2. From project root:

   **Master** (HDMI + LTC + BLE server):
   ```
   pio run -e master -t upload
   ```

   **Slave** (BLE client, no HDMI):
   ```
   pio run -e slave -t upload
   ```

   Monitor:
   ```
   pio device monitor -b 115200
   ```

### Env Comparison

| | `master` | `slave` |
|---|---|---|
| HDMI (TC358743) | ✓ | — |
| RTC (DS3231) | ✓ | — |
| OLED (SSD1306) | ✓ | — |
| MAX7219 matrix | ✓ | ✓ |
| Web UI | ✓ | ✓ (no logo) |
| BLE | Server (advertise, notify) | Client (scan, connect, subscribe) |
| LTC output | ✓ | ✓ |
| Partition scheme | `no_ota` (2 MB app) | `no_ota` (2 MB app) |

### Project Layout

```
platformio.ini
src/config.h                  pin assignments, feature toggles
src/config_master.h           includes config.h, defines BLE_MASTER (force-included in env:master via -include)
src/config_slave.h            includes config.h, defines BLE_SLAVE, disables HDMI/RTC/OLED (force-included in env:slave)
src/main.cpp                  master entry point: HDMI → LTC + BLE server
src/slave_main.cpp            slave entry point: BLE client → LTC + MAX7219 + web UI
src/webui.{h,cpp}             WiFi AP/STA, HTTP server, NVS, embedded JS/CSS/HTML
src/ble_timecode.{h,cpp}      BLE master (server/advertise/notify) & slave (scan/select/connect/subscribe)
src/ltc_encoder.{h,cpp}       esp_timer-based SMPTE-12M LTC generator
src/max7219_display.{h,cpp}   MAX7219 64×8 framebuffer driver
src/panasonic_tc.h            <-- fill in your GH5 timecode byte layout here
src/tc358743.{h,cpp}          minimal I2C driver + bitBangProbe()
src/oled_display.{h,cpp}      optional SSD1306 via U8g2
src/ds3231.{h,cpp}            optional RTC driver
src/logo_data.h               PROGMEM byte array for logo.png (excluded on BLE_SLAVE)
```

---

## WiFi

### Master

| Scenario | Behavior |
|----------|----------|
| No WiFi configured | Opens AP `TC-LTC-GENERATOR` (open, `192.168.4.1`) |
| Saved credentials exist | Connects as STA on boot; AP auto-disables on connect |
| STA disconnected >5 s | AP re-enabled for reconfiguration |

### Slave

| Scenario | Behavior |
|----------|----------|
| Default | Opens AP `TC-LTC-SLAVE` (open, `192.168.4.1`) |
| STA configured | Same auto-STA / auto-AP logic as master |

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
| **BLE (slave)** | Scan for master devices (name + address), tap to connect, view connected master name

---

## Configuration Defaults

| Setting | Master | Slave |
|---------|--------|-------|
| WiFi AP SSID | TC-LTC-GENERATOR | TC-LTC-SLAVE |
| WiFi AP security | Open | Open |
| FPS | Auto (re-detect) | 25 |
| Drop frame | Off | Off |
| RTC | Optional (DS3231) | — |
| OLED | Optional (SSD1306) | — |
| MAX7219 matrix | Optional (compile-time), enabled by default | Optional (compile-time), enabled by default |
| Matrix brightness | 4 (0–15) | 4 |
| Matrix enabled | true (runtime) | true (runtime) |
| LTC output pin | GPIO6 | GPIO6 |
| Status LED | GPIO7 | GPIO7 |
| Reverse-engineer mode | 1 (change to 0 after GH5 mapping) | 0 |
| BLE role | Master (advertise + notify) | Slave (scan + subscribe) |

---

## BLE Wireless Slave Sync

A custom 128-bit BLE service (`9a6f0001-...`) transfers timecode from master to slave as 5-byte notifications (dd, hh, mm, ss, ff):

- **Master**: advertises the service, sends a notification on every frame tick via `bleTimecodeUpdate()`. Broadcast name configurable via web UI; disconnect button removes all connected slaves.
- **Slave**: scans for devices offering the service (showing name + signal strength), taps one to connect. On receiving a timecode packet it jams the local `LtcEncoder` in real time. The web UI displays the connected master's device name.

The slave runs its own independent LTC generator, MAX7219 matrix, and web UI — exactly like the master but without HDMI/RTC/OLED hardware. BLE and WiFi share the single ESP32-C3 radio via time-division multiplexing.

### Role selection

Each PlatformIO environment force-includes the corresponding config header at compile time via `-include` (no `-D` flags in `build_flags`):

| Role | Config header | Environment |
|------|--------------|-------------|
| Master | `src/config_master.h` | `env:master` |
| Slave  | `src/config_slave.h`  | `env:slave` |

---

## Step 1: Reverse-engineer GH5 timecode bytes

`config.h` has `REVERSE_ENGINEER_MODE 1` by default. Connect the GH5 via HDMI, open serial monitor, set the GH5's timecode to a known value, then diff the InfoFrame dumps as time advances to find which bytes change. Fill in `decodeGh5Timecode()` in `src/panasonic_tc.h`, then set `REVERSE_ENGINEER_MODE 0`.

---

## Time source fallback

1. **HDMI timecode** — directly drives LTC, syncs RTC once per second
2. **DS3231 RTC** — HH:MM:SS with sub-second frame interpolation via `millis()`
3. **Free-running tick** — advances from jam time (01:00:00:00)

---

## ⚠️ GH5 4K HDMI vs TC358743

The TC358743 only supports HDMI 1.4 up to 1080p. Set the GH5's "HDMI Rec Output" to FHD in the camera menu — this project only reads InfoFrame metadata, not pixel data.

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

### TC358743 ↔ ESP32-C3 (Geekworm C790)

```
15-pin FPC → DIP breakout         ESP32-C3 Super Mini
Pin 15 CAM_3V3 ─────────────────── 3V3
Pin 1  GND     ─────────────────── GND
Pin 13 CAM_SCL ─────────────────── GPIO5 (SCL)
Pin 14 CAM_SDA ─────────────────── GPIO4 (SDA)
Pin 11 CAM_IO0 (unused or to 3V3)
Pins 2,3,5,6,8,9 (CSI-2 data/clock) — leave open
```

---

## Credits & References

- TC358743 register map: Linux kernel driver (`tc358743.c`)
- Geekworm C790: https://geekworm.com
- [MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX) LED matrix library
- LTC / SMPTE-12M: standard biphase-mark encoding

---

## License

Provided as-is for personal and educational use. Panasonic and GH5 are trademarks of Panasonic Corporation.
 