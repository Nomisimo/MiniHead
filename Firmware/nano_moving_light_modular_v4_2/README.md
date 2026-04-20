# Modular v4.2 — Clean Architecture (LittleFS + ArtnetWifi)

Sauberer Neustart auf Basis von v3. Vereinfachung des Speichersystems: NVS-Rohbinärdaten werden durch LittleFS JSON-Dateien ersetzt. Art-Net-Empfang über die ArtnetWifi-Bibliothek statt manuell geparstem UDP.

## Features

- Alles aus Modular v3 (LED, Servos, Rainbow, Cues, Sequencer, Multi-Head, Art-Net)
- **LittleFS JSON-Speicher** statt NVS: alle Daten in lesbaren `.json`-Dateien
- **ArtnetWifi-Bibliothek** statt manuellem UDP-Parser
- **Zentrales `storage.h`:** einheitliche `readJson`/`writeJson`-Helfer für alle Plugins
- Kein NVS-Versions-Management mehr — JSON-Schema ist selbsterklärend

## Gespeicherte Dateien (LittleFS)

| Datei | Inhalt |
|---|---|
| `/discovery.json` | `{"fixID": 1, "name": "Head 1"}` |
| `/cues.json` | Array aller gespeicherter Cues |
| `/artnet.json` | Array aller Art-Net-Patches |

## Aufbau

```
main/
├── main.ino               # Unverändert
├── core.h                 # + storage_begin() beim Boot
├── config.example.h       # Vorlage mit Partition-Scheme-Hinweis
└── plugins/
    ├── storage/
    │   └── storage.h      # LittleFS mount + readJson/writeJson
    ├── startup_animation/
    ├── wifi/
    │   ├── discovery.h    # /discovery.json statt NVS
    │   └── wifi_control.h # /cues.json statt NVS
    └── artnet/
        ├── artnet_receiver.h  # ArtnetWifi-Bibliothek
        └── artnet_control.h   # /artnet.json statt NVS
```

## Einrichtung

### 1. Libraries installieren

| Library | Autor | Installation |
|---|---|---|
| Adafruit NeoPixel | Adafruit | Library Manager |
| ESP32Servo | Kevin Harrington | Library Manager |
| ArduinoJson | Benoit Blanchon | Library Manager |
| ArtnetWifi | rstephan | Library Manager |

### 2. Partition Scheme setzen

**Wichtig:** Vor dem ersten Flash in Arduino IDE:

```
Tools → Partition Scheme → "Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)"
```

### 3. config.h anlegen

```bash
cp config.example.h config.h
# WIFI_SSID und WIFI_PASSWORD eintragen
```

### 4. Flashen & prüfen

Serial Monitor (115200 Baud) zeigt beim Boot:
```
[Storage] LittleFS OK
[Storage] /discovery.json — not found   ← sauberer Start
[WiFi] No cue data — starting fresh
[ArtNet] No patch data — starting fresh
```

## Unterschiede zu v3

| Aspekt | v3 | v4.2 |
|---|---|---|
| Speicher | NVS (Binary) | LittleFS (JSON) |
| Inspektion | nur per Debugger | `GET /api/config/cues` |
| Struct-Änderungen | Version-Bump + Datenverlust | einfach JSON-Feld ergänzen |
| Art-Net-Parser | manuell (30 Zeilen) | ArtnetWifi-Bibliothek |
| Globaler DMX-Buffer | `artnetDmxBuf[512]` | entfällt (Callback-Pointer) |

## Aktuelle Entwicklungsversion

Dies ist die **empfohlene Version** für neue Installationen.
