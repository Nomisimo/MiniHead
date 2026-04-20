# v1 — Serial Only

Die erste Firmware-Version. Alles in einer einzigen `.ino`-Datei. Steuerung ausschließlich über die serielle Schnittstelle.

## Features

- RGBW-LED-Steuerung (WS2812B)
- Pan & Tilt über Servos (SG90)
- Rainbow-Effekt
- Serielles Kommandoprotokoll (115200 Baud, USB-C)

## Kommandos

```
R:255,G:128,B:0,W:0
PAN:90,TILT:45
RAINBOW:1
RAINBOW:0
```

Werte werden mit Bestätigung `[OK] ...` zurückgegeben.

## Aufbau

| Datei | Inhalt |
|---|---|
| `nano_moving_light_v1.ino` | Alles — Hardware, Parser, Rainbow |

## Ziel dieser Version

Proof of Concept. Kein Netzwerk, keine Modularität. Ideal zum Testen der Hardware (Servo-Bewegungen, LED-Farben) ohne WiFi-Abhängigkeit.

## Hardware

| Pin | Komponente |
|---|---|
| 8 | WS2812B LED |
| 2 | Servo Pan |
| 3 | Servo Tilt |
