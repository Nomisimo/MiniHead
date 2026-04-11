/*
 * Nano Moving Light – v1
 * Hardware: ESP32-C3 Super Mini
 * LED:      WS2812B (1x Pixel, GPIO8)
 * Servos:   SG90 Pan (GPIO2), SG90 Tilt (GPIO3)
 * Control:  Serial ASCII über USB-C (115200 Baud)
 *
 * Protokoll (eine Zeile, \n abgeschlossen):
 *   R:255,G:128,B:0
 *   PAN:90
 *   TILT:45
 *   R:255,G:0,B:0,PAN:90,TILT:30
 *   RAINBOW:1          → Regenbogen starten
 *   RAINBOW:0          → Regenbogen stoppen
 *
 * Bibliotheken:
 *   - Adafruit NeoPixel  (Bibliotheksmanager: "Adafruit NeoPixel")
 *   - ESP32Servo         (Bibliotheksmanager: "ESP32Servo")
 */

#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

// ── Pin-Definitionen ──────────────────────────────────────────────
#define LED_PIN       8
#define LED_COUNT     1
#define SERVO_PAN_PIN 2
#define SERVO_TIL_PIN 3

// ── Servo-Grenzen (SG90: 0–180°) ─────────────────────────────────
#define SERVO_MIN 0
#define SERVO_MAX 180

// ── Objekte ───────────────────────────────────────────────────────
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
Servo servoPan;
Servo servoTilt;

// ── Zustandsvariablen ─────────────────────────────────────────────
uint8_t curR = 0, curG = 0, curB = 0, curW = 0;
int     curPan  = 90;
int     curTilt = 90;

bool    rainbowActive = false;
uint8_t rainbowHue    = 0;
unsigned long lastRainbowStep = 0;

// ── Hilfsfunktionen ───────────────────────────────────────────────

void setLED(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  strip.setPixelColor(0, strip.Color(r, g, b, w));
  strip.show();
}

// Hue (0–255) → RGB, W bleibt 0 im Regenbogen-Modus
void hueToRGB(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t sector = hue / 43;
  uint8_t offset = (hue % 43) * 6;
  switch (sector) {
    case 0: r = 255;        g = offset;      b = 0;           break;
    case 1: r = 255-offset; g = 255;         b = 0;           break;
    case 2: r = 0;          g = 255;         b = offset;      break;
    case 3: r = 0;          g = 255-offset;  b = 255;         break;
    case 4: r = offset;     g = 0;           b = 255;         break;
    default:r = 255;        g = 0;           b = 255-offset;  break;
  }
}

void setPan(int angle) {
  angle = constrain(angle, SERVO_MIN, SERVO_MAX);
  servoPan.write(angle);
  curPan = angle;
}

void setTilt(int angle) {
  angle = constrain(angle, SERVO_MIN, SERVO_MAX);
  servoTilt.write(angle);
  curTilt = angle;
}

// Einen einzelnen "KEY:VALUE"-Token verarbeiten
void parseToken(String token) {
  token.trim();
  int sep = token.indexOf(':');
  if (sep < 0) return;

  String key = token.substring(0, sep);
  String val = token.substring(sep + 1);
  key.toUpperCase();

  if      (key == "R")    { curR = (uint8_t)constrain(val.toInt(), 0, 255); }
  else if (key == "G")    { curG = (uint8_t)constrain(val.toInt(), 0, 255); }
  else if (key == "B")    { curB = (uint8_t)constrain(val.toInt(), 0, 255); }
  else if (key == "W") { curW = (uint8_t)constrain(val.toInt(), 0, 255); }
  else if (key == "PAN")  { setPan(val.toInt());  }
  else if (key == "TILT") { setTilt(val.toInt()); } 
  else if (key == "RAINBOW") {
    rainbowActive = (val.toInt() == 1);
    rainbowHue = 0;
    if (!rainbowActive) {
      // Regenbogen stoppen → LED auf letzten manuellen Wert zurück
      setLED(curR, curG, curB, curW);
      Serial.println("[OK] Regenbogen gestoppt");
    } else {
      Serial.println("[OK] Regenbogen gestartet");
    }
  }
  else {
    Serial.print("[WARN] Unbekannter Key: ");
    Serial.println(key);
  }
}

// Eine komplette Zeile parsen (komma-separierte Token)
void parseLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  bool ledChanged = false;

  // Merke aktuelle LED-Werte, um nach dem Parsen zu prüfen ob Update nötig
  uint8_t prevR = curR, prevG = curG, prevB = curB, prevW = curW;


  int start = 0;
  while (true) {
    int comma = line.indexOf(',', start);
    String token = (comma < 0) ? line.substring(start)
                               : line.substring(start, comma);
    parseToken(token);
    if (comma < 0) break;
    start = comma + 1;
  }

   // LED nur updaten wenn kein Regenbogen aktiv und sich etwas geändert hat
  if (!rainbowActive && (curR != prevR || curG != prevG || curB != prevB || curW != prevW)) {
    setLED(curR, curG, curB, curW);
  }

   // Status-Echo (nur wenn kein reiner RAINBOW-Befehl)
  if (line.indexOf("RAINBOW") < 0) {
    Serial.print("[OK] R:");  Serial.print(curR);
    Serial.print(" G:");      Serial.print(curG);
    Serial.print(" B:");      Serial.print(curB);
    Serial.print(" W:");      Serial.print(curW);
    Serial.print(" PAN:");    Serial.print(curPan);
    Serial.print(" TILT:");   Serial.println(curTilt);
  }
}




// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); // ESP32-C3: kurz warten bis USB-Serial bereit

  // NeoPixel initialisieren
  strip.begin();
  strip.setBrightness(255);
  setLED(0, 0, 0, ); // LED aus

  // Servos initialisieren
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servoPan.setPeriodHertz(50);
  servoTilt.setPeriodHertz(50);
  servoPan.attach(SERVO_PAN_PIN,  500, 2400); // SG90 Pulsbreite
  servoTilt.attach(SERVO_TIL_PIN, 500, 2400);
  setPan(90);
  setTilt(90);

  Serial.println("=== Nano Moving Light v1 bereit ===");
  Serial.println("Protokoll: R:255,G:0,B:0,PAN:90,TILT:45");
   Serial.println("Regenbogen: RAINBOW:1 / RAINBOW:0");
}

// ── Loop ──────────────────────────────────────────────────────────
String inputBuffer = "";

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      parseLine(inputBuffer);
      inputBuffer = "";
    } else if (c != '\r') {
      inputBuffer += c;
    }
  }
}
// Regenbogen-Schritt (non-blocking via millis)
  if (rainbowActive) {
    unsigned long now = millis();
    if (now - lastRainbowStep >= RAINBOW_DELAY_MS) {
      lastRainbowStep = now;
      uint8_t r, g, b;
      hueToRGB(rainbowHue, r, g, b);
      setLED(r, g, b, 0);
      rainbowHue++; // läuft automatisch von 255 auf 0 über (uint8_t overflow)
    }
  }
}
