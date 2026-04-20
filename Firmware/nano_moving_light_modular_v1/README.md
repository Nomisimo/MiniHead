# Modular v1 — Erste Modul-Architektur

Erste modulare Version. `main.ino` bleibt minimal — Features werden in eigenständige Module ausgelagert. Gleicher Funktionsumfang wie WiFi v1, aber sauber strukturiert.

## Features

- Alles aus WiFi v1 (LED, Servos, Rainbow, Cues, Sequencer, Web-UI)
- **Modulare Architektur:** Hardware (core.h) getrennt von Features (Module)
- **Startup-Animation:** Servo-Bounce + Farbwechsel beim Boot
- Manuelle Modul-Registrierung in `config.h`

## Aufbau

```
main/
├── main.ino                     # Nur Modul-Loop (29 Zeilen)
├── core.h                       # LED, Servos, Command-Parser
├── config.h                     # Modul-Registry (manuell)
└── modules/
    ├── startup_animation.h      # Boot-Animation
    └── wifi_control/
        ├── wifi_control.h       # HTTP Server, Cues, Sequencer
        └── html_page.h          # Web-UI
```

## Prinzip

```cpp
// config.h — Module manuell eintragen
Module modules[] = {
  { startup_animation_setup, startup_animation_loop },
  { wifi_control_setup,      wifi_control_loop      },
};
```

## Einrichtung

SSID/Passwort in `modules/wifi_control/wifi_control.h` ändern:

```cpp
const char* ssid     = "DEIN_NETZWERK";
const char* password = "DEIN_PASSWORT";
```

## Unterschied zu WiFi v1

Gleiche Features, bessere Struktur. `main.ino` muss nie mehr angefasst werden.
