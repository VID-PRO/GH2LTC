# [VID-PRO](https://www.vid-prot.de) GH2LTC

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
| STATUS_LED | 7 |

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

   ```
   pio run -t upload
   ```

   The same firmware supports both Master and Slave roles — switch between them at runtime via the web UI Settings panel.

   Monitor:
   ```
   pio device monitor -b 115200
   ```

### Flashing from Pre-built Binaries

Download the latest `firmware` artifact from [GitHub Releases](https://github.com/VID-PRO/GH2LTC/releases).

**Using `esptool.py`** (install via `pip install esptool`):
```
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

> Replace `/dev/ttyUSB0` with your port (`COM3` on Windows, `/dev/cu.usbmodem*` on macOS).

**Using PlatformIO** (when you have the source):
```
pio run -t upload
```

**First-time driver note (ESP32-C3 Super Mini):**  
On macOS/Linux the USB serial (CDC) should appear automatically. On Windows you may need a [CP210x or CH340 driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) depending on your board's UART bridge.

### Features by Mode

| | Master | Slave |
|---|---|---|
| HDMI (TC358743) | ✓ | — |
| RTC (DS3231) | ✓ | ✓ |
| OLED (SSD1306) | ✓ | ✓ |
| MAX7219 matrix | ✓ | ✓ |
| Web UI | ✓ | ✓ |
| BLE | Server (advertise, notify) | Client (scan, connect, subscribe) |
| LTC output | ✓ | ✓ |

The role is selected at runtime via the web UI Settings panel and saved to NVS.

### Project Layout

```
platformio.ini
src/config.h                  pin assignments, feature toggles
src/config_dual.h             includes config.h, enables runtime mode switching (force-included via -include)
src/main.cpp                  unified entry point: dispatches to master or slave logic based on runtime mode
src/webui.{h,cpp}             WiFi AP/STA, HTTP server, NVS, embedded JS/CSS/HTML
src/ble_timecode.{h,cpp}      BLE master (server/advertise/notify) & slave (scan/select/connect/subscribe)
src/ltc_encoder.{h,cpp}       esp_timer-based SMPTE-12M LTC generator
src/max7219_display.{h,cpp}   MAX7219 64×8 framebuffer driver
src/panasonic_tc.h            <-- fill in your GH5 timecode byte layout here
src/tc358743.{h,cpp}          minimal I2C driver + bitBangProbe()
src/oled_display.{h,cpp}      optional SSD1306 via U8g2
src/ds3231.{h,cpp}            optional RTC driver
src/logo_data.h               PROGMEM byte array for logo.png
```

---

## WiFi

| Scenario | Behavior |
|----------|----------|
| No WiFi configured | Opens AP with default SSID (master: `GH2LTC_XXXX`, slave: `TC-SLAVE-XXXX`), open network, `192.168.4.1` |
| Saved credentials exist | Connects as STA on boot; AP auto-disables on connect |
| STA disconnected >5 s | AP re-enabled for reconfiguration |

The AP SSID is derived from the BLE name: if the default is used (master: `TC-LTC-MASTER`, slave: `TC-SLAVE-XXXX`), the SSID is `GH2LTC_` + last 4 MAC digits (master) or `TC-SLAVE-` + last 4 MAC digits (slave). Setting a custom BLE name via the web UI replaces the SSID with that name.

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

| Setting | Master (HDMI + BLE server) | Slave (BLE client, no HDMI) |
|---------|---------------------------|-----------------------------|
| WiFi AP SSID | `GH2LTC_` + last 4 MAC digits | `TC-SLAVE-` + last 4 MAC digits |
| WiFi AP security | Open | Open |
| FPS | Auto (re-detect) | 25 |
| Drop frame | Off | Off |
| RTC | Optional (DS3231) | Optional (DS3231) |
| OLED | Optional (SSD1306) | Optional (SSD1306) |
| MAX7219 matrix | Enabled by default | Enabled by default |
| Matrix brightness | 4 (0–15) | 4 |
| Matrix enabled | true (runtime) | true (runtime) |
| LTC output pin | GPIO6 | GPIO6 |
| Status LED | GPIO7 | GPIO7 |
| Reverse-engineer mode | 0 (set to 1 during GH5 byte mapping) | 0 |
| BLE role | Master (advertise + notify) | Slave (scan + subscribe) |

---

## BLE Wireless Slave Sync

A custom 128-bit BLE service (`9a6f0001-...`) transfers timecode from master to slave as 5-byte notifications (dd, hh, mm, ss, ff):

- **Master**: advertises the service, sends a notification on every frame tick via `bleTimecodeUpdate()`. Broadcast name configurable via web UI; disconnect button removes all connected slaves.
- **Slave**: scans for devices offering the service (showing name + signal strength), taps one to connect. On receiving a timecode packet it jams the local `LtcEncoder` in real time. The web UI displays the connected master's device name.

The slave runs its own independent LTC generator, MAX7219 matrix, and web UI — exactly like the master but without HDMI/RTC/OLED hardware. BLE and WiFi share the single ESP32-C3 radio via time-division multiplexing.

### Role selection

The role is selected at runtime from the web UI Settings panel and persisted in NVS. The single firmware (`env:dual`) includes all code paths; the active one is chosen at boot based on the saved mode.

To switch modes:
1. Connect to the web UI (`http://192.168.4.1` or the ESP's STA IP).
2. Open Settings (gear icon).
3. Tap **Master** or **Slave** under Device Mode.
4. The device reboots into the selected mode.

---

## Step 1: Reverse-engineer GH5 timecode bytes

Set `REVERSE_ENGINEER_MODE 1` in `src/config.h`. Connect the GH5 via HDMI, open serial monitor, set the GH5's timecode to a known value, then diff the InfoFrame dumps as time advances to find which bytes change. Fill in `decodeGh5Timecode()` in `src/panasonic_tc.h`, then set `REVERSE_ENGINEER_MODE 0`.

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
 