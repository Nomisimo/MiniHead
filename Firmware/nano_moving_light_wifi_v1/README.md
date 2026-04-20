# WiFi v1 — Web Server

Erste WiFi-Version. Fügt einen HTTP-Server, eine Web-UI und ein Cue-System hinzu. Noch monolithisch — alles in einer `.ino`-Datei.

## Features

- Alles aus v1 (LED, Servos, Rainbow)
- WiFi-Verbindung (hardcodierte SSID/Passwort)
- Eingebettete Web-UI (HTML in `html_page.h`)
- **Cue-System:** bis zu 32 Cues speichern, abrufen, feuern
- **Sequencer:** Cues automatisch in Folge abspielen
- NVS-Persistenz für Cues
- JSON-API

## API-Routen

| Route | Methode | Funktion |
|---|---|---|
| `/` | GET | Web-UI |
| `/api/send` | POST | Kommando senden |
| `/api/cues` | GET | Alle Cues abrufen |
| `/api/cues` | POST | Cue speichern |
| `/api/cues/{id}/fire` | POST | Cue ausführen |
| `/api/cues/{id}` | DELETE | Cue löschen |
| `/api/sequencer/start` | POST | Sequenz starten |
| `/api/sequencer/status` | GET | Sequenzstatus |

## Aufbau

| Datei | Inhalt |
|---|---|
| `nano_moving_light_wifi_v1.ino` | Alles — WiFi, HTTP, Cues, Sequencer |
| `html_page.h` | Eingebettetes HTML der Web-UI |

## Einrichtung

WiFi-Credentials direkt in der `.ino`-Datei ändern:

```cpp
const char* ssid     = "DEIN_NETZWERK";
const char* password = "DEIN_PASSWORT";
```

## Hinweis

Monolithische Struktur — nicht weiter gewartet. Für Produktion → Modular v3+.
