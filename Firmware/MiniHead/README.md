# MiniHead Firmware — Modular v4.2

> Current recommended firmware for all new installations.  
> LittleFS JSON · ESPAsyncWebServer · Art-Net/DMX512 · Multi-Head UDP · BLE Provisioning

---

## Table of Contents

1. [Features](#1-features)
2. [Hardware](#2-hardware)
3. [Arduino IDE Setup](#3-arduino-ide-setup)
   - [3.1 Install Arduino IDE](#31-install-arduino-ide)
   - [3.2 Add ESP32 Board Package](#32-add-esp32-board-package)
   - [3.3 Install Libraries](#33-install-libraries)
   - [3.4 Board & Tools Settings](#34-board--tools-settings)
4. [Project Configuration — config.h](#4-project-configuration--configh)
5. [Flashing](#5-flashing)
6. [Code Structure](#6-code-structure)
   - [6.1 Entry Point](#61-entry-point--mainoino)
   - [6.2 Plugin System](#62-plugin-system)
   - [6.3 Core Hardware Layer](#63-core-hardware-layer--coreh)
   - [6.4 Plugin Overview](#64-plugin-overview)
7. [UI Themes](#7-ui-themes)
8. [HTTP API Reference](#8-http-api-reference)
   - [8.1 Control](#81-control)
   - [8.2 Cues & Sequencer](#82-cues--sequencer)
   - [8.3 Fixtures & Network](#83-fixtures--network)
   - [8.4 Art-Net Patch](#84-art-net-patch)
   - [8.5 Configuration](#85-configuration)
9. [UDP Protocol](#9-udp-protocol)
10. [Art-Net / DMX512](#10-art-net--dmx512)
11. [BLE Provisioning](#11-ble-provisioning)
12. [PC Leader App](#12-pc-leader-app)
13. [Art-Net Test Tool](#13-art-net-test-tool)
14. [Stored Files — LittleFS](#14-stored-files--littlefs)
15. [Serial Monitor — Boot Output](#15-serial-monitor--boot-output)

---

## 1. Features

| Feature | Detail |
|---|---|
| **Web UI** | Full control panel served directly from the ESP at `http://<ip>` — no app needed |
| **Multi-head network** | Leader/follower election by lowest MAC; PC Leader App always wins |
| **Cues + Sequencer** | Up to 32 saved scenes, per-fixture targeting, timed playback with loop |
| **Art-Net / DMX512** | Built-in WiFiUDP parser, 7-channel fixture footprint, no external library |
| **BLE Provisioning** | Zero-touch WiFi credential distribution via Bluetooth GATT |
| **LittleFS JSON storage** | All settings in human-readable `.json` files; survives firmware updates |
| **ESPAsyncWebServer** | Non-blocking HTTP; servo commands respond in < 5 ms |
| **WiFi multi-network** | Tries last-connected SSID first, falls back down a list |
| **Theme system** | Swap the full UI look by renaming a header file and reflashing |
| **Servo smoothing** | Exponential smoothing at 50 Hz with deadband — silent, jitter-free |
| **Profiler plugin** | Optional: loop frequency, heap, FreeRTOS task stats over Serial |

---

## 2. Hardware

| Component | Model | Pin |
|---|---|---|
| Microcontroller | ESP32-C3 Super Mini (RISC-V, single-core) | — |
| LED | WS2812B RGBW, 1 pixel | GPIO 8 |
| Pan servo | SG90 | GPIO 2 |
| Tilt servo | SG90 | GPIO 3 |
| Power | 5 V, ≥ 1 A shared rail for LED + servos | — |

> **USB CDC:** The ESP32-C3 Super Mini uses an integrated USB serial bridge — no external USB-UART adapter needed. **USB CDC On Boot must be Enabled** in Arduino IDE (see §3.4).

---

## 3. Arduino IDE Setup

### 3.1 Install Arduino IDE

Download **Arduino IDE 2.x** from https://www.arduino.cc/en/software

### 3.2 Add ESP32 Board Package

1. **File → Preferences** (macOS: **Arduino IDE → Settings**)
2. Paste into *Additional boards manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager** → search `esp32` → install **"esp32" by Espressif Systems** version **3.x**

> ⚠️ **Version 3.x is required.** Version 2.x is not compatible with the async web server fork used here.

### 3.3 Install Libraries

#### Via Library Manager (`Tools → Manage Libraries`)

| Library | Author | Notes |
|---|---|---|
| **Adafruit NeoPixel** | Adafruit | RGBW LED driver |
| **ESP32Servo** | Kevin Harrington | Servo PWM on ESP32 |
| **ArduinoJson** | Benoit Blanchon | JSON for LittleFS files and API |

> **Remove if installed (no longer needed):**
> - **ArtnetWifi** — replaced with a custom WiFiUdp parser in `artnet_receiver.h` (no external library dependency)
> - **Adafruit DMA NeoPixel** — DMA variant for SAMD21/SAMD51 only; never applicable to ESP32-C3

#### Via ZIP Install — AsyncTCP + ESPAsyncWebServer

The standard Library Manager version of ESPAsyncWebServer is **incompatible with Arduino Core 3.x**. Use the **mathieucarbou fork** exclusively.

**Download these ZIPs:**

| Library | URL |
|---|---|
| AsyncTCP | https://github.com/mathieucarbou/AsyncTCP/archive/refs/heads/main.zip |
| ESPAsyncWebServer | https://github.com/mathieucarbou/ESPAsyncWebServer/archive/refs/heads/main.zip |

**Install:** `Sketch → Include Library → Add .ZIP Library…` — install **AsyncTCP first**, then ESPAsyncWebServer.

**Remove old versions first** (if previously installed via Library Manager): delete their folders from `~/Documents/Arduino/libraries/`.

#### Libraries that come with ESP32 Arduino Core (no install needed)

These are part of the ESP32 board package and available automatically:

- `WiFi.h`, `WiFiUdp.h` — WiFi STA/AP + UDP sockets
- `ESPmDNS.h` — mDNS (`minihead.local`)
- `DNSServer.h` — Captive portal DNS
- `LittleFS.h` — Flash file system
- `BLEDevice.h`, `BLEServer.h`, `BLEClient.h`, `BLEScan.h`, … — Bluetooth GATT (BLE provisioning)
- `freertos/FreeRTOS.h`, `task.h` — RTOS tasks (BLE provisioning sender)
- `mbedtls/aes.h`, `mbedtls/md.h` — Crypto (BLE provisioning)

### 3.4 Board & Tools Settings

Set these in the **Tools** menu before every upload:

| Setting | Value |
|---|---|
| **Board** | `ESP32C3 Dev Module` |
| **USB CDC On Boot** | `Enabled` ← **required for Serial Monitor** |
| **CPU Frequency** | `160 MHz` |
| **Flash Frequency** | `80 MHz` |
| **Flash Mode** | `QIO` |
| **Flash Size** | `4MB (32Mb)` |
| **Partition Scheme** | `Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)` |
| **Upload Speed** | `921600` |
| **Port** | your ESP32's USB port |

> **Partition Scheme is mandatory.** LittleFS uses the SPIFFS partition slot. Without this, the device boots but cannot save settings — cues, patches, and WiFi memory are lost on every reset.

**Finding your port:**
- macOS: `/dev/cu.usbmodem…` or `/dev/cu.SLAB_USBtoUART`
- Windows: `COM3`, `COM4`, … (Device Manager)
- Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`

macOS CH34x driver (if port doesn't appear): https://github.com/WCHSoftGroup/ch34xser_macos

---

## 4. Project Configuration — config.h

`config.h` is **git-ignored** — copy the template first:

```bash
cp config.example.h config.h
```

`config.h` is the **only file you normally need to edit**. It controls which plugins are active and what WiFi networks to try. Here's the complete annotated file:

```cpp
// ── Plugin flags ──────────────────────────────────────────────────
// Uncomment to enable. Order here does NOT matter — plugins register
// themselves via static initializers. Execution order in main.ino is
// determined by the #include order below.

//#define PLUGIN_STARTUP_ANIMATION  // Servo sweep + color test on first boot
//#define PLUGIN_UDP_CONTROL        // Multi-head UDP discovery + leader election
//#define PLUGIN_ARTNET             // Art-Net / DMX512 receiver
//#define PLUGIN_PROFILER           // Loop timing + heap stats over Serial
//#define PLUGIN_BLE_PROVISION      // BLE GATT WiFi credential distribution

// ── WiFi network list ─────────────────────────────────────────────
// The ESP tries networks in this order (last-connected SSID gets
// priority on next boot). Leave empty if using BLE provisioning.
struct WifiCredential { const char* ssid; const char* password; };
static const WifiCredential WIFI_NETWORKS[] = {
  { "PrimarySSID",  "password1" },
  { "BackupSSID",   "password2" },  // optional fallback
};
static const int WIFI_NETWORK_COUNT =
    sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// ── AP hotspot password ───────────────────────────────────────────
// Used when no known WiFi is reachable. Must be ≥ 8 chars, or "" for open.
#define AP_PASSWORD ""

// ── BLE Provisioning ─────────────────────────────────────────────
// 32 hex chars = 16-byte AES-128 key. All devices in the fleet must
// share the same key. Change before flashing — never leave as zeros.
#define PROVISION_KEY "00000000000000000000000000000000"
```

**WiFi boot sequence:**
1. Read `/wifi_last.json` — try that SSID first if visible
2. Try remaining `WIFI_NETWORKS[]` entries in order
3. Try all networks without scan filter (hidden SSIDs)
4. After 2 failed cycles → start AP hotspot `MiniHead-XXYY` at `192.168.4.1`

**Changing config.h always requires a full re-upload** (it's compiled in). LittleFS data (cues, patches, names) is preserved across uploads.

---

## 5. Flashing

1. Connect ESP32-C3 via USB-C
2. Open `main/main.ino` in Arduino IDE
3. Verify Tools settings (§3.4) and select the correct port
4. Click **Upload** or press **Ctrl+U / Cmd+U**

Watch the console for:
```
Wrote 123456 bytes ...
Hash of data verified.
Hard resetting via RTS pin...
```

Open **Tools → Serial Monitor** at **115200 baud** and press the reset button. See §15 for expected output.

> Re-flash whenever you change `config.h`, pins, or any `#define`. LittleFS data survives.

---

## 6. Code Structure

```
main/
├── main.ino                         ← Entry point (setup + loop) — never touch this
├── config.h                         ← Your configuration (git-ignored)
├── config.example.h                 ← Template for config.h
├── core.h                           ← Hardware drivers: LED, servos, animations
├── core_globals.h                   ← extern declarations shared across plugins
├── plugin_registry.h                ← REGISTER_PLUGIN macro + _plugins[] table
└── plugins/
    ├── storage/
    │   └── storage.h                ← LittleFS mount + readJson/writeJson helpers
    ├── startup_animation/
    │   └── startup_animation.h      ← Servo sweep + color test (optional)
    ├── wifi/
    │   ├── wifi.h                   ← Plugin entry point
    │   ├── wifi_control.h           ← ESPAsyncWebServer, all HTTP routes, cues, sequencer
    │   ├── discovery.h              ← UDP beacon broadcast + leader/follower election
    │   ├── discovery_globals.h      ← Shared types: NodeRole, Peer, MAC, ports
    │   ├── discovery_stubs.h        ← Empty stubs when PLUGIN_UDP_CONTROL is off
    │   ├── html_page.h              ← Embedded main UI HTML (PROGMEM, layout only)
    │   ├── theme.h                  ← Active CSS theme (PROGMEM, served at GET /theme)
    │   ├── log_config.h             ← Runtime log level configuration
    │   └── log_panel_html.h         ← Log panel HTML
    ├── udp_control/
    │   ├── udp_control.h            ← Plugin entry point
    │   └── discovery.h              ← Leader election, peer table, UDP CMD dispatcher
    ├── artnet/
    │   ├── artnet.h                 ← Plugin entry point
    │   ├── artnet_globals.h         ← Structs, DMX_FOOTPRINT, ARTNET_PORT
    │   ├── artnet_receiver.h        ← WiFiUDP polling, DMX parsing, apply + relay
    │   ├── artnet_control.h         ← HTTP patch management routes
    │   └── artnet_panel_html.h      ← Art-Net panel HTML
    ├── ble_provision/
    │   ├── ble_provision.h          ← Full BLE GATT provisioning logic (SEEKER + SENDER)
    │   └── ble_provision_config.h   ← BLE service/characteristic UUIDs + timeouts
    ├── profiler/
    │   └── profiler.h               ← Loop timing, heap, FreeRTOS task stats (optional)
    └── shared/
        └── crypto.h                 ← AES-128-CBC + HMAC-SHA256 + key parser (shared)
```

### 6.1 Entry Point — `main.ino`

`main.ino` is intentionally minimal and **never needs to be edited** (plugins are added via `config.h`). What it does:

```
setup():
  core_setup()                → mount LittleFS, init LED + servos
  [ble_provision_pre_wifi()]  → SEEKER blocks here if no credentials (optional, BLE provisioning only)
  wifi_connectMulti()         → connect to WiFi or start AP mode
  WiFi.setSleep(false)          → disable modem sleep for low-latency UDP
  MDNS.begin("minihead")        → accessible as minihead.local
  for each plugin: setup()      → initialize all registered plugins

loop():
  DNS captive portal tick        → handles Android/iOS in AP mode
  core_loop()                   → servo smoothing, rainbow/demo animation
  for each plugin: loop()       → run all plugin loops (HTTP, Art-Net, discovery…)
```

### 6.2 Plugin System

Each plugin lives in its own folder and self-registers via a static C++ initializer before `setup()` runs:

```cpp
// At the bottom of every plugin header:
REGISTER_PLUGIN(wifi);
// expands to:
// static PluginRegistrar _wifi_reg(wifi_setup, wifi_loop);
```

The `PluginRegistrar` constructor appends `{setup, loop}` to the global `_plugins[]` array (max 8 entries). `main.ino` calls them all in registration order without knowing their names.

**To add a plugin:** uncomment its `#define` and `#include` in `config.h`.  
**To remove a plugin:** comment them out. The stubs header keeps link-time symbols valid.

### 6.3 Core Hardware Layer — `core.h`

`core.h` initializes the hardware and exposes global functions used by all plugins:

| Function | Description |
|---|---|
| `core_setup()` | Mount LittleFS, init NeoPixel, attach servos at 50 Hz |
| `core_loop()` | Servo smoothing (every 20 ms), rainbow hue cycle, demo sinusoid |
| `setLED(r, g, b, w)` | Set RGBW LED. No-op if color unchanged (saves NeoPixel interrupt) |
| `setPan(angle)` | Set pan target 0–270°. Smoothed exponentially at 50 Hz |
| `setTilt(angle)` | Set tilt target 0–270° |
| `applyCommand(String)` | Parse and apply a control string (see §9 command format) |

**Servo smoothing:** `next = current + 0.12 × (target − current)` — deadband 1.5° to stop hunting.

**Global state** (readable anywhere via `core_globals.h`):

| Variable | Type | Meaning |
|---|---|---|
| `curR/G/B/W` | `uint8_t` | Current LED color |
| `curPan/curTilt` | `int` | Current servo angle |
| `rainbowActive` | `bool` | Rainbow effect running |
| `demoActive` | `bool` | Demo sinusoid animation |
| `animSpeed` | `float` | Speed multiplier 0.1–3.0× |
| `wifiAPMode` | `bool` | Device is in AP hotspot mode |

### 6.4 Plugin Overview

| Plugin | `#define` | What it does |
|---|---|---|
| **wifi** | always on | HTTP server, web UI, cues, sequencer, leader redirect |
| **startup_animation** | `PLUGIN_STARTUP_ANIMATION` | Servo sweep + RGBW color test on boot |
| **udp_control** | `PLUGIN_UDP_CONTROL` | UDP discovery beacons, leader election, peer table, CMD relay |
| **artnet** | `PLUGIN_ARTNET` | Art-Net DMX512 receiver + HTTP patch management |
| **ble_provision** | `PLUGIN_BLE_PROVISION` | Zero-touch WiFi credential distribution via Bluetooth GATT |
| **profiler** | `PLUGIN_PROFILER` | Loop stats, heap, FreeRTOS task table over Serial |

---

## 7. UI Themes

The web UI separates **layout** (never changes) from **theme** (all colors, fonts, and visual styles). The theme is a CSS file served by the ESP at `GET /theme`.

### Available theme files

| File | Design |
|---|---|
| `theme.h` | **Active theme.** Dark cyberpunk — cyan + purple accents, Share Tech Mono |

### How to switch themes

1. Replace the contents of `theme.h` with your custom CSS (or create a new file and rename it to `theme.h`)
2. Upload firmware

No other changes needed — `wifi_control.h` always serves `theme.h` at `GET /theme`.

### How themes work (CSS variables)

Every theme defines a `:root {}` block with these variables. Components reference only `var(--name)`:

```css
:root {
  /* Backgrounds */
  --bg:        #0a0a0f;   /* page background */
  --surface:   #13131a;   /* card / panel background */
  --surface2:  #1c1c26;   /* secondary surface, inputs */
  --border:    #2a2a3a;   /* dividers, input borders */

  /* Accent colors */
  --accent:    #00e5ff;   /* primary accent (cyan / blue) */
  --accent2:   #ff6b35;   /* secondary accent (orange) */
  --accent3:   #a855f7;   /* tertiary accent (purple) */

  /* Text */
  --text:      #e0e0f0;   /* primary text */
  --text-dim:  #6b6b8a;   /* labels, metadata */

  /* Status */
  --success:   #22c55e;   /* connected, Art-Net active */
  --danger:    #ef4444;   /* error, disconnect */

  /* Typography */
  --mono:      'Share Tech Mono', monospace;
  --sans:      'Barlow', sans-serif;
}
```

To create a custom theme: copy any theme file, rename it to `theme.h`, adjust the `:root` variables to taste, and reflash.

---

## 8. HTTP API Reference

All routes are available at `http://<esp-ip>/`. Responses are `application/json` unless noted.

In AP mode (`192.168.4.1`) the full API is available. Routes guarded by leader-check redirect followers to the leader IP/port.

---

### 8.1 Control

| Method | Path | Body (JSON) | Response | Description |
|---|---|---|---|---|
| `GET` | `/api/status` | — | `{"connected":true,"port":"WiFi","ip":"…","apMode":false,"apPasswordSet":false,"rainbowActive":false,"demoActive":false,"animSpeed":1.0}` | Current device state |
| `GET` | `/api/version` | — | `{"version":"4.2"}` | Firmware version |
| `POST` | `/api/send` | `{"command":"R:255,G:0,B:0,W:0,PAN:90,TILT:45","targets":["AA:BB:CC:DD:EE:FF"]}` | `{"status":"ok"}` | Send a control command. Omit `targets` or pass `[]` for broadcast to all |
| `POST` | `/api/rainbow` | `{"on":true}` | `{"status":"ok"}` | Start or stop the rainbow hue cycle |
| `POST` | `/api/demo` | `{"on":true}` | `{"status":"ok"}` | Start or stop the sinusoid demo animation |
| `POST` | `/api/animation/speed` | `{"speed":1.5}` | `{"status":"ok"}` | Set animation speed multiplier (0.1 – 3.0) |
| `POST` | `/api/blackout` | — | `{"status":"ok"}` | Immediate blackout — LED off, stop all effects |
| `POST` | `/api/ap/password` | `{"password":"mypassword"}` | `{"status":"ok","reconnect":true}` | Set AP hotspot password (≥ 8 chars). Empty string removes it |

**Command string format** (used in `POST /api/send` and UDP CMD):

```
R:0-255,G:0-255,B:0-255,W:0-255,PAN:0-270,TILT:0-270
RAINBOW:0|1
DEMO:0|1
SPEED:0.1-3.0
BLACKOUT
```

Fields are optional and can be combined: `R:255,PAN:90` sets red and pan without touching other channels.

---

### 8.2 Cues & Sequencer

A **cue** is a named scene: RGBW color + pan + tilt + a list of fixture targets.

| Method | Path | Body (JSON) | Response | Description |
|---|---|---|---|---|
| `GET` | `/api/cues` | — | `[{"id":1700000000,"name":"Red Wash","r":255,"g":0,"b":0,"w":0,"pan":90,"tilt":45,"fixTargets":[1,2],"targetCount":2}, …]` | List all saved cues |
| `POST` | `/api/cues` | `{"name":"My Cue","r":255,"g":128,"b":0,"w":0,"pan":90,"tilt":45,"fixTargets":[1]}` | `{"status":"ok","id":1700000001}` | Save current state as a new cue |
| `POST` | `/api/cues/:id/fire` | — | `{"status":"ok"}` | Fire cue immediately to its targets |
| `PUT` | `/api/cues/:id/targets` | `{"fixTargets":[1,2,3]}` | `{"status":"ok"}` | Update which fixtures a cue targets |
| `PUT` | `/api/cues/reorder` | `{"order":[id3,id1,id2]}` | `{"status":"ok"}` | Reorder cues (affects sequencer playback order) |
| `DELETE` | `/api/cues/:id` | — | `{"status":"ok"}` | Delete cue by ID |
| `POST` | `/api/sequencer/start` | `{"interval_ms":2000,"loop":true,"cue_ids":[id1,id2,id3]}` | `{"status":"ok"}` | Start automatic sequence playback |
| `POST` | `/api/sequencer/stop` | — | `{"status":"ok"}` | Stop sequencer |
| `GET` | `/api/sequencer/status` | — | `{"running":false}` | Is sequencer currently playing? |

**Cue limits:** max 32 cues, max 16 fixture targets per cue. `fixTargets:[]` or `fixTargets:[0]` targets all known fixtures. Cues persist in `/cues.json` across reboots.

---

### 8.3 Fixtures & Network

| Method | Path | Body (JSON) | Response | Description |
|---|---|---|---|---|
| `GET` | `/api/heads` | — | `[{"mac":"AA:BB:CC:DD:EE:FF","ip":"192.168.1.100","fixID":1,"name":"Head 1","role":"LEADER","online":true}, …]` | All known heads including self |
| `GET` | `/api/fixtures` | — | `[{"id":1,"name":"Head 1","mac":"AA:BB:CC:DD:EE:FF","ip":"192.168.1.100","online":true}, …]` | Fixtures indexed by fixID |
| `POST` | `/api/heads/:mac/identify` | `{"on":true}` | `{"status":"ok"}` | Flash LED white for 2 s to identify a head |
| `POST` | `/api/heads/:mac/fixid` | `{"fixID":2}` | `{"status":"ok"}` | Set fixture ID on a remote head |
| `POST` | `/api/heads/:mac/name` | `{"name":"Stage Left"}` | `{"status":"ok"}` | Set display name on a remote head |

---

### 8.4 Art-Net Patch

Only available when `PLUGIN_ARTNET` is enabled.

| Method | Path | Body (JSON) | Response | Description |
|---|---|---|---|---|
| `GET` | `/api/artnet/status` | — | `{"active":true,"patchCount":1,"r":255,"g":0,"b":0,"w":0,"pan":90,"tilt":45}` | Art-Net status + live DMX output |
| `GET` | `/api/artnet/patch` | — | `[{"fixID":1,"universe":0,"startAddr":1}]` | Current patch assignments |
| `POST` | `/api/artnet/patch` | `{"universe":0,"startAddr":1}` | `{"status":"ok"}` | Set own patch |
| `PUT` | `/api/artnet/patch/*` | `{"universe":0,"startAddr":8}` | `{"status":"ok"}` | Update universe/address (pass -1 to leave unchanged) |
| `DELETE` | `/api/artnet/patch` | — | `{"status":"ok"}` | Clear all patches |
| `POST` | `/api/artnet/patch/bulk` | `{"universe":0,"startAddr":1,"count":4,"firstFixID":1}` | `{"status":"ok"}` | Auto-assign patches to N consecutive fixtures starting at firstFixID |

**DMX fixture footprint (7 channels):**

| Ch offset | Parameter | Range |
|---|---|---|
| 0 | Master dimmer | 0–255 |
| 1 | Red | 0–255 |
| 2 | Green | 0–255 |
| 3 | Blue | 0–255 |
| 4 | White | 0–255 |
| 5 | Pan | 0–255 → 0–270° |
| 6 | Tilt | 0–255 → 0–270° |

---

### 8.5 Configuration

These routes always operate on the local device (no leader redirect):

| Method | Path | Body (JSON) | Response | Description |
|---|---|---|---|---|
| `POST` | `/api/config/fixid` | `{"fixID":1}` | `{"status":"ok"}` | Set this device's fixture ID (persists to `/discovery.json`) |
| `POST` | `/api/config/name` | `{"name":"Stage Left"}` | `{"status":"ok"}` | Set this device's display name |
| `GET` | `/api/logconfig` | — | log config JSON | Get runtime log levels |
| `POST` | `/api/logconfig` | log config JSON | `{"status":"ok"}` | Set runtime log levels |

---

## 9. UDP Protocol

When `PLUGIN_UDP_CONTROL` is enabled, devices broadcast discovery beacons and listen for commands.

### Discovery beacon — port 4210

Sent every 2000 ms as a UTF-8 string broadcast to the subnet:

```
MINIHEAD|AA:BB:CC:DD:EE:FF|192.168.1.100|1|LEADER|Head 1|UDP
          ─────MAC──────── ──────IP───── ^ ──────  ──────  ───
                                         fixID    name    mode
```

`mode` is `UDP` or `ARTNET`. `role` is `LEADER` or `FOLLOWER`.

A peer is considered stale after **90 seconds** without a beacon.

### Commands — port 4211

```
CMD|AA:BB:CC:DD:EE:FF|R:255,G:0,B:0,W:0,PAN:90,TILT:45
     ──target MAC───  ──────command string──────────────
```

Special packets:

```
IDENTIFY_ON|AA:BB:CC:DD:EE:FF    → flash LED white for 2 s
IDENTIFY_OFF|AA:BB:CC:DD:EE:FF   → stop identify immediately
SETPATCH|fixID|universe|addr      → set Art-Net patch on target
```

### Leader election

1. Each device listens on port 4210 for 4 seconds at boot
2. Lowest MAC wins — `00:00:00:00:00:PC` (PC Leader App) always wins
3. Followers redirect their web UI requests to the leader
4. If leader goes silent for > 90 s, a new election happens after a 10 s hold

---

## 10. Art-Net / DMX512

Enable with `#define PLUGIN_ARTNET` in `config.h`.

- Listens on UDP port **6454** (Art-Net standard)
- Receives **ArtDMX** packets (opcode `0x5000`)
- Applies channels to the local fixture based on the **patch** (universe + start address)
- Relays commands to peer heads via UDP CMD when Art-Net controls multiple fixtures
- Times out after **8 seconds** without a packet → marks Art-Net inactive, re-enables manual control
- Status bar in the web UI turns green when Art-Net is active; manual sliders gray out

**Setup:**
1. Enable `PLUGIN_ARTNET`, compile, flash
2. Open `http://<esp-ip>` → Art-Net panel → set Universe and Start Address
3. Send Art-Net from a controller (PC App, test tool, DMX desk, etc.)

---

## 11. BLE Provisioning

Enable with `#define PLUGIN_BLE_PROVISION` in `config.h`.

This allows devices with **no WiFi credentials** (empty `WIFI_NETWORKS[]`) to receive the SSID and password wirelessly from a device that is already connected — no USB cable or AP needed for new heads. Uses Bluetooth GATT (frequency-hopping 2.4 GHz) instead of ESP-NOW, so there is **no WiFi channel constraint**.

### Roles

| Role | Condition | Behaviour |
|---|---|---|
| **SEEKER** | `WIFI_NETWORK_COUNT == 0` and no `/wifi_provision.json` | BLE scan loop; on finding a SENDER: connects, reads characteristic, verifies HMAC, decrypts, saves credentials, reboots |
| **SENDER** | Connected to WiFi via STA | Starts a GATT server in a background task; advertises encrypted SSID+PW for `PROVISION_SENDER_MS`; then deinits BLE automatically |

### Security

- **PROVISION_KEY** (set in `config.h`, 32 hex chars) is used as AES-128-CBC key and HMAC-SHA256 key
- Only devices with the matching key can decrypt the payload
- SSID+PW are AES-128-CBC encrypted with a fresh random IV per payload
- HMAC-SHA256 covers the full payload — integrity and authentication
- Key is never transmitted — only used locally for crypto operations

### Setup

1. Set a real key in `config.h`:
   ```cpp
   #define PROVISION_KEY "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6"
   ```
2. Uncomment `#define PLUGIN_BLE_PROVISION`
3. Flash **all devices** with the same binary
4. One device needs `WIFI_NETWORKS[]` filled in (it becomes the SENDER)
5. Other devices start with empty `WIFI_NETWORKS[]` → they become SEEKERs

### Boot flow with provisioning enabled

```
Device with credentials:            Device without credentials:
  boot → ble_provision_pre_wifi()     boot → ble_provision_pre_wifi()
  → already has creds → no-op         → SEEKER: BLE scan loop
  wifi_connectMulti() → connected       ↓ finds SENDER GATT service
  plugin setup() → SENDER task starts   ↓ reads + verifies payload
  advertises encrypted creds → →      credentials verified (HMAC + AES decrypt)
  stops after PROVISION_SENDER_MS     saved to /wifi_provision.json
                                        ESP.restart()
                                        boot → wifi_connectMulti() → connected ✓
                                        plugin_setup() → also SENDER now
```

---

## 12. PC Leader App

`PC APP/pc_leader.py` — Python/Flask server that acts as the permanent network leader. ESP devices detect it and yield to it automatically.

### Requirements

```bash
pip install flask
```

### Start

```bash
cd "PC APP"
python3 pc_leader.py
```

Open **http://localhost:8080**

### What it does

| Function | Detail |
|---|---|
| Leader beacon | UDP broadcast every 1 s → ESP yields to PC |
| Peer discovery | Reads ESP beacons → builds peer table |
| Full web UI | Control all heads from `http://localhost:8080` |
| Art-Net sniffer | Monitors UDP 6454 for live DMX |
| Patch push | Sends `SETPATCH` UDP to ESP when patch changes |
| Auto-redirect | Visiting `http://<esp-ip>` while PC App is running redirects to `http://127.0.0.1:8080` |

---

## 13. Art-Net Test Tool

`PC APP/artnet_test.py` — standalone DMX sender with a browser UI for testing fixtures.

```bash
pip install flask
python3 artnet_test.py                             # broadcast
python3 artnet_test.py 192.168.1.100 0 1           # unicast to IP, universe 0, addr 1
python3 artnet_test.py 192.168.1.255 0 1 --bind 192.168.1.50  # force outgoing interface
```

Open **http://localhost:8765**

| Terminal key | Action |
|---|---|
| `SPACE` | Toggle demo mode |
| `n` | Toggle rainbow |
| `0` | Blackout |
| `f` | Full white |
| `p / P` | Pan ± 10 |
| `t / T` | Tilt ± 10 |
| `r/g/b/w` / `R/G/B/W` | Color channel ± 10 |
| `m / M` | Master ± 10 |
| `q` | Quit |

---

## 14. Stored Files — LittleFS

All files survive firmware uploads. To wipe, use **Tools → ESP32 Sketch Data Upload** with an empty data folder, or call `LittleFS.format()` once.

| File | Format | Contents |
|---|---|---|
| `/wifi_last.json` | `{"ssid":"YourSSID"}` | Last successfully connected SSID |
| `/wifi_provision.json` | `{"ssid":"…","pass":"…"}` | Credentials received via BLE provisioning (deleted after successful connect) |
| `/discovery.json` | `{"fixID":1,"name":"Head 1"}` | Fixture ID and display name |
| `/cues.json` | JSON array | Up to 32 saved cues |
| `/artnet.json` | JSON array | Art-Net patch assignments |
| `/config.json` | `{"apPassword":"…"}` | AP password override (set via API) |

---

## 15. Serial Monitor — Boot Output

Set baud to **115200**. Press the reset button after opening.

**Successful STA connect:**
```
[Storage] LittleFS OK
[Core] LED + Servos ready
[WiFi] Scanning...
[WiFi] Connected: YourSSID  IP: 192.168.1.100
[Discovery] MAC: AA:BB:CC:DD:EE:FF  FixID: 1
[Discovery] Listening 4 s for existing leader...
[Discovery] ** I am the LEADER **
[WiFi] Async server started
[ArtNet] Listening on port 6454
```

**AP mode fallback:**
```
[WiFi] All networks failed (2/2)
[WiFi] AP mode — SSID: MiniHead-EEFF  IP: 192.168.4.1
[WiFi] Captive portal DNS started
[WiFi] mDNS: minihead.local
```

**SEEKER provisioning (first boot, no credentials):**
```
[PROVISION] No credentials — SEEKER mode (BLE scan)
[PROVISION] SEEKER BLE scan...
[PROVISION] SENDER found: AA:BB:CC:DD:EE:FF
[PROVISION] Payload received 124 bytes — verifying
[PROVISION] Credentials OK — SSID: YourSSID
[Storage] /wifi_provision.json — written
[PROVISION] Saved — rebooting
--- (reboot) ---
[WiFi] Trying "YourSSID" .....
[WiFi] Connected: YourSSID  IP: 192.168.1.101
```

**Art-Net active:**
```
[ArtNet] Fix#1  M=255 R=216 G=255 B=0 W=0  PAN=41 TILT=125
[ArtNet] Timeout — inactive
```
