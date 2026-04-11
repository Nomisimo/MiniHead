# 💡 Mini LED Moving Head

> Ein DIY-Projekt für einen selbst gebauten Mini-LED-Moving-Head auf Basis von Microcontrollern.

---

## 📖 Beschreibung

Dieses Projekt dient zur Entwicklung eines miniaturisierten LED-Moving-Heads – einer kleinen motorgesteuerten Lichteinheit, wie sie in der Bühnentechnik verwendet wird. Ziel ist eine kostengünstige, selbst aufgebaute Variante für Hobby, Bühne oder Experimentierprojekte.

---

## 🙏 Danksagung

Dieses Projekt basiert auf der hervorragenden Arbeit von **cooke001** und seinem [miniTheatreProject](https://github.com/cooke001/miniTheatreProject), das als Inspiration und Grundlage dient. Wir danken dem Entwickler herzlich für die Bereitstellung seines Projekts als Open-Source-Ressource.

---

## 📋 Materialien

| Komponente | Modell | Anzahl |
|---|---|---|
| Microcontroller | ESP32-C3 Super Mini | 1x |
| LED | WS2812B RGBW (1 Pixel) | 1x |
| Servo Pan | SG90 | 1x |
| Servo Tilt | SG90 | 1x |

> *(Liste wird im Verlauf des Projekts ergänzt)*

---

## ✨ Features

- [x] Pan- und Tilt-Bewegung über Servo-Motoren
- [x] Steuerung über WLAN / Web UI (ESP32)
- [x] Farbwechsel (RGBW-LED-Support)
- [x] Standalone-Betrieb mit vorprogrammierten Sequenzen (Cues)
- [x] Startup Animation
- [x] Modulares Firmware-System
- [ ] DMX512-Schnittstelle
- [ ] Helligkeitssteuerung per PWM
- [ ] 3D-druckbares Gehäuse (STL-Dateien)

---

## 🚀 Erste Schritte

### 1. Repository klonen

```bash
git clone https://github.com/Nomisimo/MiniHead.git
cd MiniHead
```

### 2. Arduino IDE einrichten

- Arduino IDE 2.x installieren
- ESP32 Board-Support installieren: `Preferences → Additional Boards Manager URLs`:
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```
- Board: **ESP32C3 Dev Module**

### 3. Libraries installieren

Über den Library Manager (`Sketch → Include Library → Manage Libraries`):

| Library | Autor |
|---|---|
| Adafruit NeoPixel | Adafruit |
| ESP32Servo | Kevin Harrington |
| ArduinoJson | Benoit Blanchon (v7) |

### 4. WiFi Credentials eintragen

In `firmware/modules/wifi_control/wifi_control.h`:

```cpp
const char* WIFI_SSID     = "DEIN_NETZWERK";
const char* WIFI_PASSWORD = "DEIN_PASSWORT";
```

### 5. Partition Scheme setzen

`Tools → Partition Scheme → Huge APP (3MB No OTA)`

### 6. Uploaden & verbinden

- Sketch uploaden
- Serial Monitor öffnen (115200 Baud)
- IP-Adresse ablesen und im Browser öffnen

---

## 📁 Projektstruktur

```
MiniHead/
├── firmware/
│   ├── main.ino                        # Einstiegspunkt — nie anfassen
│   ├── config.h                        # Modul-Registry
│   ├── core.h                          # LED, Servos, Command-Parser
│   └── modules/
│       ├── startup_animation.h         # Startanimation
│       └── wifi_control/
│           ├── wifi_control.h          # HTTP Server, Cues, Sequencer
│           └── html_page.h             # Web UI (eingebettet)
├── hardware/                           # Schaltpläne, Stückliste
├── 3d-models/                          # STL-Dateien für den Druck
├── docs/                               # Weitere Dokumentation
└── README.md
```

---

## 🧩 Modul-System

Die Firmware ist modular aufgebaut. `main.ino` bleibt immer unverändert — neue Funktionen werden als eigenständige Module hinzugefügt.

### Neues Modul hinzufügen

**1.** Datei `modules/mein_modul.h` anlegen mit zwei Pflichtfunktionen:

```cpp
void mein_modul_setup() { /* einmalig beim Start */ }
void mein_modul_loop()  { /* jeden Frame */         }
```

**2.** In `config.h` eintragen:

```cpp
#include "modules/mein_modul.h"

Module modules[] = {
  { core_setup,         core_loop         },
  { mein_modul_setup,   mein_modul_loop   },  // ← neue Zeile
};
```

Das war's.

---

## 🤝 Mitmachen

Beiträge sind willkommen! Bitte öffne zuerst ein Issue, um größere Änderungen zu besprechen.

1. Fork erstellen
2. Feature-Branch anlegen (`git checkout -b feature/mein-feature`)
3. Änderungen committen (`git commit -m 'Add: mein Feature'`)
4. Branch pushen (`git push origin feature/mein-feature`)
5. Pull Request öffnen

---

## 📜 Lizenz

Dieses Projekt steht unter der **GNU General Public License v3.0**.  
Weitere Informationen: [LICENSE](./LICENSE) oder [gnu.org/licenses/gpl-3.0](https://www.gnu.org/licenses/gpl-3.0.html)