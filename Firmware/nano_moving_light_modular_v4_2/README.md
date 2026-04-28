# Modular v4.2 — Setup & Developer Guide

> Current recommended firmware for new installations.
> LittleFS JSON storage · ESPAsyncWebServer · WiFi multi-network · Built-in Art-Net parser · PC Leader App

---

## Table of Contents

1. [Features](#1-features)
2. [Hardware](#2-hardware)
3. [Arduino IDE — Board Package](#3-arduino-ide--board-package)
4. [Library Installation](#4-library-installation)
5. [Tools Settings (Board, Partition, Baud)](#5-tools-settings)
6. [Project Configuration (config.h)](#6-project-configuration-configh)
7. [Flashing the ESP32-C3](#7-flashing-the-esp32-c3)
8. [PC Leader App](#8-pc-leader-app)
9. [UI Themes](#9-ui-themes)
10. [Art-Net Test Tool](#10-art-net-test-tool)
11. [Verify Boot — Serial Monitor](#11-verify-boot--serial-monitor)
12. [Stored Files (LittleFS)](#12-stored-files-littlefs)
13. [File Structure](#13-file-structure)
14. [Differences to v3](#14-differences-to-v3)

---

## 1. Features

- **LittleFS JSON storage** — all settings in readable `.json` files, no NVS binary blobs
- **ESPAsyncWebServer** (mathieucarbou fork) — non-blocking HTTP; servo commands respond in < 5 ms
- **WiFi multi-network list** — tries last-connected network first, falls back down the list
- **Built-in Art-Net / DMX512** — WiFiUDP polling, no external Art-Net library needed
- **PC Leader App** (`pc_leader.py`) — Python/Flask; the PC acts as network leader, the ESP as follower
- **Theme system** — drop a `.css` file in `PC APP/themes/` to switch UI designs; ESP theme lives in `theme.h`
- **Art-Net status bar** — green bar in web UI when DMX is active; controls gray out automatically
- **Multi-Head** — leader/follower election by lowest MAC; PC always wins with `00:00:00:00:00:PC`
- **Cues + Sequencer** — saved to LittleFS; fire individually or run as timed sequence
- **Profiler plugin** (optional) — loop frequency, heap, FreeRTOS task table

---

## 2. Hardware

| Component      | Model                    | Notes                          |
|----------------|--------------------------|--------------------------------|
| Microcontroller | ESP32-C3 Super Mini     | RISC-V single-core, USB-C      |
| LED            | WS2812B RGBW (1 pixel)   | Pin 8                          |
| Servo Pan      | SG90                     | Pin 2                          |
| Servo Tilt     | SG90                     | Pin 3                          |
| Power          | 5 V, ≥ 1 A              | Shared rail for LED + servos   |

> **USB CDC note:** The ESP32-C3 Super Mini uses a built-in USB serial bridge.
> No external USB-UART adapter needed. Make sure **USB CDC On Boot** is **Enabled** in Arduino IDE (see §5).

---

## 3. Arduino IDE — Board Package

### 3.1 Install Arduino IDE

Download Arduino IDE 2.x from https://www.arduino.cc/en/software

### 3.2 Add ESP32 Board URL

1. Open **File → Preferences** (macOS: **Arduino IDE → Settings**)
2. Paste into **Additional boards manager URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Click **OK**

### 3.3 Install ESP32 Board Package

1. Open **Tools → Board → Boards Manager**
2. Search: `esp32`
3. Find **"esp32" by Espressif Systems** — install version **3.x** (IDF 5.x)
   > ⚠️ Version 3.x is required. Version 2.x will not compile the async server correctly.

---

## 4. Library Installation

### 4.1 Library Manager (automatic)

Open **Tools → Manage Libraries** (or **Sketch → Include Library → Manage Libraries**):

| Library              | Author               | Search term        |
|----------------------|----------------------|--------------------|
| Adafruit NeoPixel    | Adafruit             | `NeoPixel`         |
| ESP32Servo           | Kevin Harrington     | `ESP32Servo`       |
| ArduinoJson          | Benoit Blanchon      | `ArduinoJson`      |

Install each one — latest version is fine for all three.

### 4.2 ZIP Install — AsyncTCP + ESPAsyncWebServer

The standard Library Manager version of ESPAsyncWebServer is **not compatible** with Arduino Core 3.x / IDF 5.x. Use the **mathieucarbou fork** only.

**Step 1 — Download ZIPs:**

| Library            | Download link |
|--------------------|---------------|
| AsyncTCP           | https://github.com/mathieucarbou/AsyncTCP/archive/refs/heads/main.zip |
| ESPAsyncWebServer  | https://github.com/mathieucarbou/ESPAsyncWebServer/archive/refs/heads/main.zip |

**Step 2 — Install in Arduino IDE:**

1. **Sketch → Include Library → Add .ZIP Library…**
2. Select `AsyncTCP-main.zip` → Open
3. Repeat for `ESPAsyncWebServer-main.zip`

> Install **AsyncTCP first**, then ESPAsyncWebServer (it depends on AsyncTCP).

**Step 3 — Remove old versions (if any):**

If you previously installed `AsyncTCP` or `ESPAsyncWebServer` from the Library Manager,
remove them first: delete their folders from `~/Documents/Arduino/libraries/`.

---

## 5. Tools Settings

Select these in the **Tools** menu before compiling or uploading:

| Setting               | Value                                              |
|-----------------------|----------------------------------------------------|
| **Board**             | `ESP32C3 Dev Module`                               |
| **USB CDC On Boot**   | `Enabled` ← **critical for Serial Monitor**        |
| **CPU Frequency**     | `160 MHz`                                          |
| **Flash Frequency**   | `80 MHz`                                           |
| **Flash Mode**        | `QIO`                                              |
| **Flash Size**        | `4MB (32Mb)`                                       |
| **Partition Scheme**  | `Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)` |
| **Upload Speed**      | `921600`                                           |
| **Port**              | *(select your ESP32's USB port)*                   |

> **Partition Scheme is mandatory.** LittleFS uses the SPIFFS slot. Without this scheme the
> device boots but cannot write settings — cues, patches, and WiFi memory will be lost on reset.

### How to find your port

- **macOS:** `/dev/cu.usbmodem…` or `/dev/cu.SLAB_USBtoUART`
- **Windows:** `COM3`, `COM4`, … (check Device Manager)
- **Linux:** `/dev/ttyUSB0` or `/dev/ttyACM0`

On macOS you may need to install the CH34x driver if the port does not appear:
https://github.com/WCHSoftGroup/ch34xser_macos

---

## 6. Project Configuration (config.h)

The file `config.h` is **not tracked by git** (in `.gitignore`). Create it from the template:

```bash
cp config.example.h config.h
```

Then edit `config.h` and fill in your WiFi credentials:

```cpp
struct WifiCredential { const char* ssid; const char* password; };

static const WifiCredential WIFI_NETWORKS[] = {
  { "YourPrimarySSID",  "password1" },
  { "YourBackupSSID",   "password2" },   // optional second network
};
static const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);
```

**WiFi connection logic at boot:**
1. Reads `/wifi_last.json` — tries the last-connected SSID first
2. If that SSID is visible → connects immediately
3. Scans for remaining entries in order
4. If nothing found → tries all SSIDs without scan filter
5. Total timeout ≈ 30 s; retries forever until connected

---

## 7. Flashing the ESP32-C3

### First flash

1. Connect the ESP32-C3 via USB-C
2. Confirm the port appears in Tools → Port
3. Open `main/main.ino` in Arduino IDE
4. Click **Upload** (→ button) or press **Ctrl+U**

The IDE compiles, links, and flashes automatically. Watch the bottom console for:
```
Connecting........_____....
Chip is ESP32-C3 ...
Wrote 123456 bytes ...
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

### Verify with Serial Monitor

After flashing:
1. **Tools → Serial Monitor**
2. Set baud rate to **115200** (bottom-right dropdown in the Serial Monitor)
3. Press the reset button on the ESP32-C3

Expected boot output:
```
[MiniHead] Modular v4.2 — LittleFS + AsyncUDP
[Storage] LittleFS OK
[Core] LED + Servos ready
[WiFi] Scanning...
[WiFi] Connected: YourSSID  IP: 192.168.1.100
[Discovery] MAC: AA:BB:CC:DD:EE:FF  FixID: 0
[Discovery] Listening for 4s to find existing nodes...
[Discovery] ** I am the LEADER **
[WiFi] IP: 192.168.1.100
[WiFi] Async server started
[ArtNet] Listening on port 6454
```

> If you see `[WiFi] ERROR: failed to bind` or a crash dump, re-check the **USB CDC On Boot**
> and **Partition Scheme** settings, then re-flash.

### Re-flash after settings change

Changing `WIFI_NETWORKS`, pin assignments, or any `#define` in config.h requires a full re-upload.
LittleFS data (cues, patches, discovery settings) is **preserved** across uploads — only a
**Tools → ESP32 Sketch Data Upload** or explicit LittleFS format would erase it.

---

## 8. PC Leader App

The PC App (`PC APP/pc_leader.py`) runs on your Mac/PC and acts as the network leader.
The ESP becomes a follower once it hears the PC's beacon.

### Requirements

- Python 3.8 or newer
- Flask

```bash
pip install flask
```

### Running

```bash
cd "PC APP"
python3 pc_leader.py
```

Expected output:
```
[PC Leader] PC App v4.2
[PC Leader] MAC:  00:00:00:00:00:PC
[PC Leader] IP:   192.168.1.50
[PC Leader] Open: http://localhost:8080
[PC Leader] Beaconing on port 4210, CMD on port 4211
[ArtNet] Sniffer on UDP 6454 (broadcast only)
[Discovery] Peer joined: AA:BB:CC:DD:EE:FF  IP:192.168.1.100  Fix#1  "Head 1"  FOLLOWER
[ArtNet] Join-sync SETPATCH:1,0,1 → 192.168.1.100
```

Open **http://localhost:8080** in your browser.

### What the PC App does

| Function            | Detail                                              |
|---------------------|-----------------------------------------------------|
| Leader beacon       | UDP broadcast every 1 s → ESP yields to PC         |
| Peer discovery      | Reads ESP beacons → builds peer table              |
| Patch push          | Sends `SETPATCH` UDP to ESP on join or patch change |
| Art-Net sniffer     | Listens on UDP 6454 (broadcast) for live DMX data  |
| Web UI              | Full control panel at `http://localhost:8080`       |

> **Redirect:** If you open `http://<esp-ip>` in a browser while the PC App is running,
> the ESP detects the request came from the leader's machine and redirects you to
> `http://127.0.0.1:8080` automatically.

---

## 9. UI Themes

The UI is split into a layout layer (`css/style.css`) that never changes, and a theme layer that controls all visual styles.

### PC App

The active theme is the **alphabetically first `.css` file** found in `PC APP/themes/`.

| File | Design |
|------|--------|
| `custom.css` | Dark cyberpunk redesign — **default (active)** |
| `minimal.css` | Clean light design |
| `original.css` | Exact pre-v4.2 look (Share Tech Mono, scanline overlay) |
| `session.css` | Simpler dark design built early in the v4.2 session |

**To switch themes:** delete or rename the files you don't want so the desired one sorts first. No server restart needed — the theme is loaded fresh on each page load.

**To create a custom theme:** add a new `.css` file to `PC APP/themes/`. It only needs to define `:root` variables and any visual overrides — the layout CSS handles all positioning.

### ESP

The ESP serves its theme from `plugins/wifi/theme.h` at `GET /theme`. Edit the PROGMEM CSS string in that file and reflash to change the ESP UI appearance.

---

## 10. Art-Net Test Tool

`PC APP/artnet_test.py` is a standalone DMX sender + browser UI for testing.

### Requirements

```bash
pip install flask
```

### Usage

```bash
# Broadcast to all devices (default)
python3 artnet_test.py

# Unicast to specific ESP
python3 artnet_test.py 192.168.1.100 0 1
#                      TARGET_IP      ^  ^
#                                UNIVERSE  START_ADDR (1-based)

# Force a specific outgoing network interface (useful with VPN/Docker)
python3 artnet_test.py 192.168.1.255 0 1 --bind 192.168.1.50
```

Open **http://localhost:8765** for the browser control panel.

### Terminal keys (while running)

| Key     | Action                    |
|---------|---------------------------|
| `SPACE` | Toggle demo mode          |
| `n`     | Toggle rainbow            |
| `0`     | Blackout                  |
| `f`     | Full white                |
| `h`     | Highlight                 |
| `r/R`   | Red ±10                   |
| `g/G`   | Green ±10                 |
| `b/B`   | Blue ±10                  |
| `w/W`   | White ±10                 |
| `m/M`   | Master ±10                |
| `p/P`   | Pan ±10                   |
| `t/T`   | Tilt ±10                  |
| `q`     | Quit                      |

> **macOS with VPN or Docker:** The tool auto-detects the correct outgoing interface
> and binds the socket to it, preventing Art-Net from routing through a tunnel.
> If packets still don't reach the ESP, use `--bind <your-LAN-ip>` explicitly.

---

## 11. Verify Boot — Serial Monitor

### Art-Net active (patch matched)

When an Art-Net packet arrives and matches the fixture's patch, the ESP prints only
when the output values **change** (not on every 40 Hz tick):

```
[ArtNet] Fix#1  M=255 R=216 G=255 B=0 W=0  PAN=41 TILT=125
```

### Art-Net timeout

After **8 seconds** without a packet the ESP marks Art-Net inactive:
```
[ArtNet] Timeout — inactive
```

### Discovery / role change

```
[Discovery] Heard: AA:BB:CC:DD:EE:FF  IP:192.168.1.50  Fix#0  LEADER  "PC"
[Discovery] Leader signal lost — holding...
[Discovery] ** I am the LEADER **
```

---

## 12. Stored Files (LittleFS)

| File               | Contents                                      |
|--------------------|-----------------------------------------------|
| `/discovery.json`  | `{"fixID": 1, "name": "Head 1"}`              |
| `/artnet.json`     | Array of Art-Net patch records                |
| `/cues.json`       | Array of saved cues                           |
| `/wifi_last.json`  | `{"ssid": "YourSSID"}` — last connected SSID  |

Files survive firmware updates. To reset everything, use:
**Tools → ESP32 Sketch Data Upload** with an empty data folder, or send `LittleFS.format()` once via Serial.

---

## 13. File Structure

```
main/
├── main.ino                    # Entry point — setup() / loop()
├── config.h                    # Your WiFi credentials (git-ignored)
├── config.example.h            # Template
├── core.h                      # LED, Servo, rainbow, command parser
├── core_globals.h              # extern declarations shared across plugins
├── plugin_registry.h           # REGISTER_PLUGIN macro
└── plugins/
    ├── storage/
    │   └── storage.h           # LittleFS mount + readJson / writeJson
    ├── startup_animation/
    ├── wifi/
    │   ├── wifi.h              # Plugin entry point
    │   ├── wifi_connect.h      # Multi-network WiFi connection logic
    │   ├── wifi_control.h      # ESPAsyncWebServer + all HTTP routes
    │   ├── discovery.h         # UDP beacon, leader election, peer table
    │   ├── discovery_globals.h # extern peer table, ownMAC, nodeRole
    │   ├── udp_control.h       # UDP CMD receiver (port 4211)
    │   ├── html_page.h         # Embedded main UI HTML — layout CSS only (PROGMEM)
    │   ├── theme.h             # Visual theme CSS served at GET /theme (PROGMEM)
    │   ├── discovery_panel_html.h
    │   └── artnet_panel_html.h (via artnet/)
    └── artnet/
        ├── artnet.h            # Plugin entry point
        ├── artnet_globals.h    # Structs, channel offsets, ARTNET_PORT
        ├── artnet_receiver.h   # WiFiUDP polling, DMX apply + relay
        └── artnet_control.h    # HTTP routes for patch management

PC APP/
├── pc_leader.py                # Flask server + UDP leader
├── index.html                  # Main UI (links external CSS/JS)
├── css/
│   └── style.css               # Layout/structural CSS only
├── js/
│   └── app.js                  # UI JavaScript
├── themes/                     # Drop a .css file here to switch theme
│   ├── custom.css              # Dark cyberpunk (default — active)
│   ├── minimal.css             # Clean light design
│   ├── original.css            # Pre-v4.2 original design
│   └── session.css             # Simpler dark design from v4.2 session
└── plugins/
    ├── wifi/                   # Discovery panel
    ├── artnet/                 # Art-Net patch panel
    └── log/                    # Log config panel
```

---

## 14. Differences to v3

| Aspect             | v3                            | v4.2                                      |
|--------------------|-------------------------------|-------------------------------------------|
| Storage            | NVS binary                    | LittleFS JSON — human-readable            |
| Data inspection    | Debugger only                 | Open `.json` files directly               |
| Schema changes     | Version bump + data loss      | Add JSON field, backward compatible       |
| Art-Net library    | ArtnetWifi (external)         | Built-in WiFiUDP parser — no dependency   |
| HTTP server        | WebServer (blocking)          | ESPAsyncWebServer (non-blocking)          |
| Servo latency      | 50–200 ms                     | < 5 ms                                    |
| WiFi               | Single SSID hardcoded         | List + last-connected priority            |
| PC integration     | None                          | PC Leader App (Python/Flask)              |
| Art-Net status UI  | None                          | Green bar + control lock in web UI        |
