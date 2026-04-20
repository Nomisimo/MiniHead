# Modular v2 — Plugin-System + Multi-Head-Netzwerk

Einführung des Auto-Registrierungs-Plugin-Systems und Multi-Head-Netzwerkfähigkeit. Mehrere ESP32-Geräte finden sich automatisch im Netzwerk und koordinieren sich.

## Features

- Alles aus Modular v1
- **Auto-Registrierung:** Plugins registrieren sich selbst via `REGISTER_PLUGIN` Makro
- **Multi-Head-Netzwerk:** Leader/Follower-Election via UDP-Beacons
- **FixID:** Jedes Gerät hat eine eigene Fixture-ID
- **Cue-Targets:** Cues können an bestimmte Fixtures adressiert werden
- **UDP-Steuerung:** Leader sendet Kommandos an Follower

## Aufbau

```
main/
├── main.ino               # Unverändert — iteriert Plugins
├── plugin_registry.h      # Auto-Registrierungs-System
├── core.h                 # LED, Servos, Command-Parser
├── config.h               # Nur noch includes, kein manuelles Array
└── plugins/
    ├── startup_animation/ # Boot-Animation
    └── wifi/
        ├── wifi.h         # Plugin-Einstiegspunkt
        ├── discovery.h    # UDP-Beacons + Leader-Election
        ├── wifi_control.h # HTTP Server, Cues, Sequencer
        └── udp_control.h  # UDP Sender/Empfänger
```

## Plugin-System

```cpp
// Plugin registriert sich selbst — keine Änderung in config.h nötig:
REGISTER_PLUGIN(discovery);

// config.h: nur noch includes
#include "plugins/startup_animation/startup_animation.h"
#include "plugins/wifi/wifi.h"
```

## Netzwerk-Rollen

| Rolle | Verhalten |
|---|---|
| `ROLE_LEADER` | Betreibt HTTP Server, verteilt Kommandos an Follower |
| `ROLE_FOLLOWER` | Kein HTTP Server, empfängt UDP-Kommandos vom Leader |

Der Leader wird per MAC-Adresse gewählt (kleinste MAC gewinnt). Ein PC mit fake-MAC `00:00:00:00:00:PC` gewinnt immer.

## Einrichtung

Credentials in `config.h` (aus `config.example.h` kopieren):

```cpp
#define WIFI_SSID     "DEIN_NETZWERK"
#define WIFI_PASSWORD "DEIN_PASSWORT"
```
