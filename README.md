# 💡 Mini LED Moving Head

> Ein DIY-Projekt für einen selbst gebauten Mini-LED-Moving-Head auf Basis von Microcontrollern.

---

## 📋 Hardware

| Komponente | Modell |
|---|---|
| Microcontroller | ESP32-C3 Super Mini |
| LED | WS2812B RGBW (1 Pixel) |
| Servo Pan | SG90 |
| Servo Tilt | SG90 |

---

## 🚀 Schnellstart

```bash
git clone https://github.com/Nomisimo/MiniHead.git
```

→ Aktuelle Firmware: **[Modular v4.2](Firmware/nano_moving_light_modular_v4_2/README.md)**

---

## 📦 Firmware-Versionen

| Version | Beschreibung | Status |
|---|---|---|
| [v1 — Serial Only](Firmware/nano_moving_light_v1/README.md) | Serielle Steuerung, kein Netzwerk | Legacy |
| [WiFi v1](Firmware/nano_moving_light_wifi_v1/README.md) | Web-UI, Cues, Sequencer (monolithisch) | Legacy |
| [Modular v1](Firmware/nano_moving_light_modular_v1/README.md) | Erste Modul-Architektur | Legacy |
| [Modular v2](Firmware/nano_moving_light_modular_v2/README.md) | Plugin-System, Multi-Head-Netzwerk | Legacy |
| [Modular v3](Firmware/nano_moving_light_modular_v3/README.md) | Art-Net / DMX512 | Legacy |
| [**Modular v4.2**](Firmware/nano_moving_light_modular_v4_2/README.md) | LittleFS JSON, ESPAsyncWebServer, WiFi-Multi-Network, Built-in Art-Net, PC Leader App, Theme System | ✅ Aktuell |

---

## 🙏 Danksagung

- **cooke001** — [miniTheatreProject](https://github.com/cooke001/miniTheatreProject) (Inspiration & Grundlage)
- **mathieucarbou** — [ESPAsyncWebServer](https://github.com/mathieucarbou/ESPAsyncWebServer) / [AsyncTCP](https://github.com/mathieucarbou/AsyncTCP) (IDF 5.x compatible forks)

> Art-Net™ is a trademark of Artistic Licence Holdings Ltd.

---

## 📜 Lizenz

**GNU General Public License v3.0** — [LICENSE](./LICENSE)

---

*Vibe coded with [Claude Code](https://claude.ai/code)*
