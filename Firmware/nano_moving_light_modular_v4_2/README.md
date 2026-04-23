# Modular v4.2 — Clean Architecture

Sauberer Neustart auf Basis von v3. NVS-Rohbinärdaten werden durch LittleFS JSON ersetzt, der synchrone WebServer durch ESPAsyncWebServer, und WiFi unterstützt eine konfigurierbare Netzwerkliste.

## Features

- Alles aus Modular v3 (LED, Servos, Rainbow, Cues, Sequencer, Multi-Head, Art-Net)
- **LittleFS JSON-Speicher** statt NVS — alle Daten in lesbaren `.json`-Dateien
- **ESPAsyncWebServer** statt synchronem WebServer — HTTP läuft auf dem WiFi-Task, blockiert nie den Arduino-Loop → Servo-Befehle reagieren sofort
- **WiFi-Multi-Network-Liste** in `config.h` — letztes verbundenes Netzwerk wird zuerst versucht, Fallback auf weitere Einträge
- **ArtnetWifi-Bibliothek** statt manuellem UDP-Parser
- **Zentrales `storage.h`:** einheitliche `readJson`/`writeJson`-Helfer für alle Plugins
- **Temporäres Profiler-Plugin** für Serial-Diagnose (Loop-Freq, Heap, FreeRTOS-Tasks)

## Gespeicherte Dateien (LittleFS)

| Datei | Inhalt |
|---|---|
| `/discovery.json` | `{"fixID": 1, "name": "Head 1"}` |
| `/cues.json` | Array aller gespeicherter Cues |
| `/artnet.json` | Array aller Art-Net-Patches |
| `/wifi_last.json` | `{"ssid": "MeinNetz"}` — letztes erfolgreich verbundenes Netzwerk |

## Aufbau

```
main/
├── main.ino               # wifi_connectMulti() statt WiFi.begin()
├── core.h                 # storage_begin() beim Boot
├── config.example.h       # Vorlage mit WIFI_NETWORKS[]
└── plugins/
    ├── storage/
    │   └── storage.h      # LittleFS mount + readJson/writeJson
    ├── startup_animation/
    ├── profiler/          # temporäres Diagnose-Plugin (nach Diagnose löschen)
    │   └── profiler.h
    ├── wifi/
    │   ├── discovery.h        # /discovery.json statt NVS
    │   └── wifi_control.h     # ESPAsyncWebServer, /cues.json statt NVS
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
| **ESPAsyncWebServer** | mathieucarbou | ZIP-Install (siehe unten) |
| **AsyncTCP** | mathieucarbou | ZIP-Install (siehe unten) |

> **Hinweis ESPAsyncWebServer:** Nicht über den Library Manager installieren — nur die mathieucarbou-Fork ist mit ESP32 Arduino Core 3.x / IDF 5.x kompatibel. Als ZIP manuell installieren:
> 1. [AsyncTCP ZIP](https://github.com/mathieucarbou/AsyncTCP/archive/refs/heads/main.zip) herunterladen
> 2. [ESPAsyncWebServer ZIP](https://github.com/mathieucarbou/ESPAsyncWebServer/archive/refs/heads/main.zip) herunterladen
> 3. Arduino IDE: **Sketch → Include Library → Add .ZIP Library** → erst AsyncTCP, dann ESPAsyncWebServer

### 2. Partition Scheme setzen

**Wichtig:** Vor dem ersten Flash in Arduino IDE:

```
Tools → Partition Scheme → "Default 4MB with spiffs (1.2MB APP / 1.5MB SPIFFS)"
```

LittleFS nutzt den SPIFFS-Slot. Ohne dieses Scheme bootet das Gerät nicht korrekt.

### 3. config.h anlegen

```bash
cp config.example.h config.h
```

WiFi-Netzwerke eintragen — mehrere möglich:

```cpp
struct WifiCredential { const char* ssid; const char* password; };
static const WifiCredential WIFI_NETWORKS[] = {
  { "PrimärNetz",  "Passwort1" },
  { "BackupNetz",  "Passwort2" },   // optional
};
static const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);
```

Der ESP prüft beim Boot:
1. Welches Netzwerk zuletzt verbunden war (`/wifi_last.json`)
2. Ob dieses Netzwerk gerade sichtbar ist → versucht es zuerst
3. Fallback auf weitere Einträge in Listenreihenfolge
4. Falls kein bekanntes SSID sichtbar: alle ohne Scan-Filter probieren
5. Bei Totalausfall: 10 s warten, neu scannen

### 4. Flashen & prüfen

Serial Monitor (115200 Baud) zeigt beim Boot:

```
[Storage] LittleFS OK
[WiFi] Scanning...
[WiFi] 3 network(s) found
[WiFi] Trying "PrimärNetz" .....
[WiFi] Connected: PrimärNetz  IP: 192.168.1.100
[WiFi] Async server started
[ArtNet] No patch data — starting fresh
```

## Profiler-Plugin (temporär)

Zum Diagnoseieren der ESP-Auslastung: eine Zeile ans **Ende** von `config.h` hinzufügen:

```cpp
#include "plugins/profiler/profiler.h"   // TEMP — nach Diagnose entfernen
```

Gibt alle 5 Sekunden im Serial Monitor aus: Loop-Frequenz, Frame-Zeit, Heap, WiFi RSSI, FreeRTOS-Task-Tabelle mit Stack-Auslastung.

Nach der Diagnose: Zeile entfernen, Ordner `plugins/profiler/` löschen.

## Unterschiede zu v3

| Aspekt | v3 | v4.2 |
|---|---|---|
| Speicher | NVS (Binary) | LittleFS (JSON) |
| Daten-Inspektion | nur per Debugger | direkt lesbare JSON-Dateien |
| Struct-Änderungen | Version-Bump + Datenverlust | JSON-Feld einfach ergänzen |
| Art-Net-Parser | manuell (30 Zeilen) | ArtnetWifi-Bibliothek |
| HTTP-Server | WebServer (blocking) | ESPAsyncWebServer (non-blocking) |
| Servo-Latenz | 50–200 ms (blockiert durch HTML-Responses) | < 5 ms (läuft auf WiFi-Task) |
| WiFi | einzelnes Netzwerk hardcoded | Liste + last-connected Priorität |

## Aktuelle Entwicklungsversion

Dies ist die **empfohlene Version** für neue Installationen.
