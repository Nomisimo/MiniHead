# Modular v3 — Art-Net / DMX512

Erweiterung um professionelle DMX512-Steuerung via Art-Net. Jeder Knoten empfängt Art-Net-Pakete unabhängig und wendet seine zugewiesenen Kanäle an.

## Features

- Alles aus Modular v2
- **Art-Net / DMX512:** Empfang auf UDP-Port 6454
- **Patch-System:** Fixture-ID → Universe → Startkanal
- **7 DMX-Kanäle** pro Fixture: Master, R, G, B, W, Pan, Tilt
- **Leader-Relay:** Leader verteilt Art-Net-Daten an Follower via UDP CMD
- **Patch-Persistenz:** NVS-Speicherung der Patch-Konfiguration
- **Web-UI für Patching:** Direktes Zuweisen von Universes/Kanälen

## DMX-Kanal-Belegung

| Kanal | Offset | Funktion |
|---|---|---|
| 1 | +0 | Master Dimmer (skaliert R/G/B/W) |
| 2 | +1 | Rot |
| 3 | +2 | Grün |
| 4 | +3 | Blau |
| 5 | +4 | Weiß |
| 6 | +5 | Pan (0–255 → 0–180°) |
| 7 | +6 | Tilt (0–255 → 0–180°) |

## Art-Net-Setup

1. Patch anlegen: Web-UI → Art-Net → Fixture-ID + Universe + Startkanal
2. Lighting Console (QLab, Resolume, MA2 etc.) auf gleichen Universe konfigurieren
3. Daten senden → Gerät reagiert sofort

## Aufbau

```
plugins/
├── wifi/            # Wie v2 (Discovery, HTTP, UDP)
└── artnet/
    ├── artnet.h             # Plugin-Einstiegspunkt
    ├── artnet_receiver.h    # UDP-Empfang + Packet-Parser (manuell)
    ├── artnet_control.h     # HTTP-Routen + NVS-Speicher
    ├── artnet_globals.h     # Konstanten, Structs, Externs
    └── artnet_panel_html.h  # Web-UI Patch-Panel
```

## Speicherung

Patches werden in NVS gespeichert (Namespace `"artnet"`, versioniert). Cues ebenfalls in NVS.

## Hinweis

Dies ist die letzte Version mit manuellem Art-Net-UDP-Parser und NVS-Rohbinär-Speicherung. Für eine sauberere Implementierung → **v4.2**.
