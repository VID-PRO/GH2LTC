# [VID-PRO](https://www.vid-pro.de) TC-WL

Reads Panasonic GH5 timecode from HDMI via TC358743 and regenerates it as SMPTE-12M LTC audio. Three PlatformIO environments: **TC-WL-HDMI** (Waveshare ESP32-P4-WIFI6, HDMI receiver, BLE server), **TC-WL-LTC** (Seeed Studio XIAO ESP32-C3, dual-role: BLE server with LTC input or BLE client with LTC output), and **TC-WL-CLAP** (ESP32-C3, BLE client, LED matrix + OLED). TC-WL-HDMI has no LED matrix — it uses a different GPIO layout from the LTC/CLAP boards.

---

## Features

| Category | Details |
|----------|---------|
| **HDMI timecode capture** | Reads GH5 InfoFrame metadata over I2C via TC358743 — no video decoding needed |
| **LTC generation** | Standalone SMPTE-12M biphase-mark encoder, esp_timer-driven, independent of I2C polling |
| **Frame rates** | Auto-detected from HDMI (24/25/30/50/60 fps) or manual via web UI |
| **RTC fallback** | Optional DS3231 preserves accurate time across power cycles with frame interpolation |
| **LED matrix (CLAP)** | 8 daisy-chained MAX7219 8×8 modules (64×8 px), software SPI; runtime toggle in web UI; not available on HDMI or LTC (GPIO conflict with buttons) |
| **OLED display (optional)** | 128×64 SSD1306 on shared I2C bus: device name, battery gauge + runtime, big timecode, master/`F`/lock/`B` indicator, FPS mode/rate, LTC mode — controlled via 4 physical buttons on HDMI/LTC; CLAP shows main screen only (no buttons) |
| **Web UI** | Fullscreen dark-teal SPA: timecode display, Auto/fixed FPS config, jam sync, brightness slider, matrix on/off, WiFi config |
| **WiFi** | AP on boot; auto-STA connect to saved network; AP re-enables on disconnect |
| **Reverse-engineer mode** | Dumps InfoFrame packets over serial to find GH5's exact timecode byte layout |
| **BLE wireless sync** | HDMI advertises timecode via BLE notify; LTC/CLAP scan by service UUID, subscribe to notifications, run local LTC + display |

---

## Hardware

### Bill of Materials

#### TC-WL-HDMI (Waveshare ESP32-P4-WIFI6)
| Component | Notes | Buy |
|-----------|-------|-----|
| **Waveshare ESP32-P4-WIFI6** | ESP32-P4 + ESP32-C6 companion for WiFi/BLE | [waveshare.com](https://www.waveshare.com) |
| **TC358743 HDMI→CSI-2** | e.g. Geekworm C790 — I2C + CSI-2 | widely available |
| **22-pin to 15-pin CSI ribbon cable** | Connects TC358743 to ESP32-P4-WIFI6 CSI connector | search "22-pin to 15-pin CSI cable" |
| **DS3231 RTC (optional)** | Battery-backed, I2C, ±2ppm | any electronics supplier |
| **128×64 OLED SSD1306 (optional)** | I2C, shares bus with TC358743 + RTC | any electronics supplier |
| **3.5mm TRS jack** | LTC audio output | any electronics supplier |
| **R1: 1kΩ, C1: 4.7nF, C2: 1µF** | LTC low-pass + DC block | — |
| **R3, R4: 10kΩ** | LTC level pad | — |
| **CR2032 coin cell** | Backup for DS3231 | any electronics supplier |
#### TC-WL-LTC (Seeed Studio XIAO ESP32-C3)

| Component | Notes |
|-----------|-------|
| **Seeed Studio XIAO ESP32-C3** | 400 MHz RISC-V, USB-C |
| **MAX7219 8×8 LED matrix** | 8 daisy-chained modules (64×8 px) |
| **DS3231 RTC (optional)** | I2C |
| **128×64 OLED SSD1306 (optional)** | I2C |
| **3.5mm TRS jack** | LTC audio output |
| **Passives** | Same RC filter as HDMI board |

### Pinout

| Function | TC-WL-HDMI (ESP32-P4) | TC-WL-LTC (ESP32-C3) | TC-WL-CLAP (ESP32-C3) |
|----------|----------------------|----------------------|----------------------|
| **I2C SDA** | GPIO 7 | GPIO 4 | GPIO 4 |
| **I2C SCL** | GPIO 8 | GPIO 5 | GPIO 5 |
| **I2C devices** | TC358743 `0x0F`, OLED `0x3C`, DS3231 `0x68` | OLED `0x3C`, DS3231 `0x68` | OLED `0x3C` |
| **MAX7219 DIN** | — | — (GPIO 2 used for button) | GPIO 2 |
| **MAX7219 CS** | — | — (GPIO 3 used for button) | GPIO 3 |
| **MAX7219 CLK** | — | — | GPIO 10 |
| **LTC output** | GPIO 6 | GPIO 6 | GPIO 6 |
| **LTC input (master)** | — | GPIO 7 | — |
| **Battery ADC (LiPo)** | GPIO 4 | GPIO 0 (A0) | — |
| **Button UP** | GPIO 10 | GPIO 8 | — |
| **Button DOWN** | GPIO 9 | GPIO 9 | — |
| **Button OK** | GPIO 2 | GPIO 2 | — |
| **Button CANCEL** | GPIO 3 | GPIO 3 | — |
| **TC358743 reset** | GPIO -1 (unused) | — | — |
| **CSI connector** | 22-pin to TC358743 | — | — |

---

## Software

### Environments

| Env | Board | Role | BLE | Platform |
|-----|-------|------|-----|----------|
| `TC-WL-LTC` | Seeed Studio XIAO ESP32-C3 | Dual-role: master (BLE server + LTC input) or slave (BLE client + LTC output), OLED + RTC, physical buttons, OLED menu; MAX7219 matrix not supported (GPIO conflict with buttons) | ✓ (native C3) | `pioarduino/platform-espressif32`† |
| `TC-WL-CLAP` | ESP32-C3 Super Mini | BLE client, LED matrix + OLED, no physical buttons | ✓ (native C3) | `pioarduino/platform-espressif32`† |
| `TC-WL-HDMI` | ESP32-P4-WIFI6 | HDMI receiver, BLE server, physical buttons + OLED menu | via C6 coprocessor‡ (ESP-Hosted SDIO) | `pioarduino/platform-espressif32`† |

† Pinned to GitHub: `https://github.com/pioarduino/platform-espressif32.git` (needed for ESP32-P4 `esp_timer` API compatibility; also used by LTC/CLAP for consistency)
‡ ESP32-P4 has no native BLE controller. The Waveshare board's ESP32-C6 companion provides WiFi/BLE over SDIO via ESP-Hosted firmware (pre-flashed). All BLE code uses preprocessor guards (`SOC_BLE_SUPPORTED \|\| CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE`) to compile correctly on P4.

### Building

```bash
# LTC (ESP32-C3, full I/O)
pio run -e TC-WL-LTC -t upload

# CLAP (ESP32-C3, LED matrix only)
pio run -e TC-WL-CLAP -t upload

# HDMI (ESP32-P4-WIFI6)
pio run -e TC-WL-HDMI -t upload
```

Monitor:
```bash
pio device monitor -b 115200
```

### Project Layout

```
platformio.ini
src/config.h                   pin assignments, feature toggles, battery runtime default (common base)
src/config_tcwl_ltc.h             -include for TC-WL-LTC env
src/config_tcwl_clap.h            -include for TC-WL-CLAP env
src/config_tcwl_hdmi.h            -include for TC-WL-HDMI env
src/main.cpp                   compile-time dispatch: TC-WL-HDMI/LTC/CLAP paths
src/webui/                     WiFi AP/STA, HTTP server, NVS, embedded JS/CSS/HTML
src/hdmi/                      TC358743 I2C driver + register map + GH5 timecode decoder
src/ltc/                       esp_timer-based SMPTE-12M LTC generator
src/matrix/                    MAX7219 64×8 framebuffer driver (CLAP only)
src/oled/                      optional SSD1306 via U8g2
src/rtc/                       optional DS3231 RTC driver
src/timecode/                  BLE HDMI (advertise/notify) & LTC (scan/select/connect/subscribe)
3DPrints/                     STL/3MF enclosures for TC-WL-HDMI (ESP32-P4) and TC-WL-CLAP (ESP32-C3 + LED matrix)
```

---

## WiFi

| Scenario | Behavior |
|----------|----------|
| No WiFi configured | Opens AP with default SSID (HDMI: `TC-WL-HDMI-XXXX`, LTC: `TC-WL-LTC-XXXX`, CLAP: `TC-WL-CLAP-XXXX`), open network, `192.168.4.1` |
| Saved credentials exist | Connects as STA on boot; AP auto-disables on connect |
| STA disconnected >5 s | AP re-enabled for reconfiguration |

---

## Web Interface

Open `http://192.168.4.1` (AP mode) or the ESP's STA IP. The header displays a centered **VID-PRO** link (https://www.vid-pro.de).

| Feature | Description |
|---------|-------------|
| **Timecode display** | Fullscreen `dd:hh:mm:ss:ff` with color-coded status dot (green=HDMI, orange=RTC, gray=free) |
| **FPS config** | Auto (re-detect from HDMI) or fixed 24/25/30/50/60, drop-frame toggle, saved to NVS |
| **Jam sync** | Set timecode manually (dd:hh:mm:ss:ff) |
| **Brightness (LTC/CLAP)** | Slider for MAX7219 intensity (0–15), saved to NVS |
| **Matrix toggle (LTC/CLAP)** | Enable/disable LED matrix display at runtime, saved to NVS |
| **WiFi config** | SSID/password input, saved to NVS, forget option |
| **BLE (HDMI)** | Change broadcast name, view connected client count, disconnect all |
| **BLE (LTC master)** | Same server controls as HDMI (change server name, view/disconnect clients), plus LTC decoder status |
| **BLE (LTC/CLAP slave)** | Scan for HDMI or LTC-master server devices (name + address), tap to connect, view server name, BLE status on OLED (`F` = not synced, `B` = synced) |

---

## Configuration Defaults

| Setting | HDMI (BLE server, no matrix) | LTC (dual-role master/slave, no HDMI) | CLAP (LED matrix + OLED) |
|---------|------------------------------|----------------------------|---------------------------|
| **WiFi AP SSID** | `TC-WL-HDMI-` + last 4 MAC digits | `TC-WL-LTC-` + last 4 MAC digits | `TC-WL-CLAP-` + last 4 MAC digits |
| **Battery ADC** | GPIO 4 (`BAT_ADC_PIN = 4`) | GPIO 0 (`BAT_ADC_PIN = 0`) | Disabled |
| **Battery runtime** | `BAT_FULL_RUNTIME_MIN = 600` (10 h) | `BAT_FULL_RUNTIME_MIN = 600` (10 h) | N/A |
| FPS | Auto (re-detect) | Auto (re-detect†) | Auto (re-detect†) |
| Drop frame | Off | Off | Off |
| RTC | Optional (DS3231) | Optional (DS3231) | Optional (DS3231) |
| OLED | Optional (SSD1306) | Optional (SSD1306) | Optional (SSD1306) — main screen only (no buttons) |
| MAX7219 matrix | Disabled (no hardware) | Disabled (GPIO conflict with buttons) | Enabled by default |
| Matrix brightness | N/A | N/A | 4 |
| LTC output pin | GPIO6 | GPIO6 | GPIO6 |
| LTC input pin (master) | — | GPIO7 | — |
| Physical buttons + OLED menu | GPIO 10/9/2/3 (UP/DOWN/OK/CANCEL) | GPIO 8/9/2/3 (UP/DOWN/OK/CANCEL) | — |
| TC_RESET_PIN | -1 (unused) | — | — |
| Reverse-engineer mode | 0 (set to 1 in `config_tcwl_hdmi.h`) | — | — |
| BLE role | Server (advertise + notify) | Configurable: master (server + LTC input) or slave (client + LTC output) | Client (scan + subscribe) |

† Relies on HDMI frame-sync on HDMI; no-op on LTC/CLAP (falls back to configured FPS).

---

## OLED Display Layout

The 128×64 SSD1306 display is organized in three fixed zones (HDMI, LTC, and CLAP builds):

```
┌─ Top line (8×13 font) ──────────────────────────────┐
│ ≡  Device Name (centered)          [||||] 10h      │
│    wifi icon                       battery  runtime │
├─ Timecode (logisoso18, centered) ────────────────────┤
│                    88:88:88:88                      │
├─ Bottom line (6×10, 4 bordered boxes) ──────────────┤
│ [H] [A] [25fps] [LTC OUT]                          │
│  └─ master indicator   └─ FPS mode/rate  └─ LTC     │
│  or F / lock / B                                     │
└──────────────────────────────────────────────────────┘
```

* **Box 1 (14 px):** Shows `H` (HDMI master), `L` (LTC master mode), `F` (CLAP/LTC slave, not synced), lock icon (LTC slave, BLE synced), or `B` (CLAP, BLE synced)
* **Box 2 (16 px):** `A` (auto FPS) or `M` (manual FPS)
* **Box 3 (42 px):** Framerate — `24fps`, `25fps`, `30fps`, `50fps`, `60fps`
* **Box 4 (50 px):** LTC mode — `LTC OUT` or `LTC IN`

A 4-button menu (UP/DOWN/OK/CANCEL) overlays the main screen when active (HDMI and LTC only — CLAP has no physical buttons). Menu items are build-specific:
* **LTC:** FPS, Drop Frame, LTC Mode, Matrix, Brightness, OLED, Exit
* **HDMI:** FPS, Drop Frame, LTC Out, OLED, Exit

## Battery Monitoring

TC-WL-HDMI and TC-WL-LTC support single-cell LiPo monitoring via a voltage divider on the ADC pin (CLAP has no battery ADC):
- **TC-WL-HDMI:** GPIO 4
- **TC-WL-LTC:** GPIO 0 (A0)

The ADC (12-bit, 11 dB attenuation) is read every 10 s; voltage is converted to 0–100 % over the 3.3–4.2 V range using `BAT_DIVIDER` (default 2.0 f for a 200k:200k divider). Remaining runtime is estimated from `batteryPct × BAT_FULL_RUNTIME_MIN / 100` and displayed as `Xh` (≥1 h) or `Xm` (<1 h) right of the battery icon. Set `BAT_FULL_RUNTIME_MIN` in config to match your battery and load.

## BLE Wireless Sync

A custom 128-bit BLE service (`9a6f0001-...`) transfers timecode from HDMI to LTC/CLAP as 5-byte notifications (dd, hh, mm, ss, ff):

- **HDMI**: advertises the service, sends a notification on every frame tick via `bleTimecodeUpdate()`. Broadcast name configurable via web UI; disconnect button removes all connected clients.
- **TC-WL-LTC (master)**: same BLE server role as HDMI — receives LTC audio via GPIO 7 decoder, advertises timecode over BLE. No HDMI hardware needed; can act as a standalone LTC-to-BLE bridge for slave units.
- **LTC/CLAP (slave)**: scans for devices offering the service (showing name + address), taps one to connect. On receiving a timecode packet it jams the local `LtcEncoder` in real time. The web UI displays the connected server's device name.

LTC hardware runs its own LTC generator, MAX7219 matrix, OLED, web UI, and physical buttons with on-device menu. In master mode it decodes LTC from GPIO 7 and acts as a BLE timecode server; in slave mode it receives timecode via BLE and generates standalone LTC output. CLAP is client-only (no LTC input/output, LED matrix + OLED main screen, no physical buttons). The HDMI board has no MAX7219 hardware and uses the OLED for status display plus a menu system controlled by four physical buttons.

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

**WebUI JS crashes on CLAP** — if the config drawer shows `—` for BLE status and the name input stays empty, the browser may be loading a cached version of the page from before the JS `typeof` guard fix. Hard-refresh (Cmd+Shift+R) to load the updated script. The issue was a `ReferenceError` on `oledToggle`/`ltcToggle` (excluded by `#ifndef TCWL_CLAP`) that propagated out of an IIFE and halted the entire `<script>` block before `pollBleLtc()` could register.

---

## Schematics

### LTC Output

```
GPIO6 (LTC_OUT_PIN)
  └───[ R1: 1k ]───┬───[ C2: 1µF ]───┬─── TRS TIP
                    │                 │
                 [ C1: 4.7nF ]    [ R2: 20k ]
                    │                 │
                   GND               │
                                     │
GND ────────────────────────────────┴─── TRS SLEEVE
```

### LTC Input (TC-WL-LTC master mode)

Connect a 3.3V-tolerant LTC source (e.g., another TC-WL's LTC output or a
professional LTC generator) to GPIO 7.  Because most LTC sources produce
audio-level signals (±1 Vpp), a simple NPN transistor pre-amplifier is needed
to convert the signal to clean 0/3.3 V logic for the ESP32-C3 GPIO.

```
                    +3.3V
                      │
                  [R5] 10k
                      │
                      ├── GPIO 7
                      │
                  C ──┤
                      │
    ──────────┬──[C3]── B ────<   BC547 (NPN)
              │       │
              │   E ──┤
              │       │
              │     GND
              │
          [R6] 100k
              │
             GND
              ▲
              │
TRS TIP ──────┘  10µF DC block

TRS SLEEVE ────── GND
```

| Part | Value | Purpose |
|------|-------|---------|
| C3 | 10 µF electrolytic | DC block — removes bias from incoming audio |
| R5 | 10 kΩ | Pull-up to 3.3 V (holds GPIO high when transistor off) |
| R6 | 100 kΩ | Base pulldown (keeps transistor off with no signal) |
| Q1 | BC547 (NPN) | Switches on positive half-cycles of LTC signal |

**Signal path:** Audio from TRS TIP passes through C3 to the base of Q1.
When the input rises above ≈0.7 V, Q1 turns on, pulling GPIO 7 low.
When below threshold, Q1 turns off, and R5 pulls GPIO 7 high.
The circuit inverts and clips the LTC signal — the biphase-mark transitions
are preserved and the ESP32-C3 ISR triggers on both edges, so polarity does
not matter.

For a simpler (but less robust) test, a direct connection may work if the
LTC source swings rail-to-rail (0–3.3 V).  Most professional LTC outputs
are 1 Vpp and *require* the amplifier above.

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
