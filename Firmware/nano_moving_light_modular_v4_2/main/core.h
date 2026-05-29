#pragma once
#define FIRMWARE_VERSION "Modular v4.2 — LittleFS + AsyncUDP"
#include "plugin_registry.h"
#include "plugins/storage/storage.h"

// ── Core Module ───────────────────────────────────────────────────
// Handles: RGBW LED output, Servo Pan/Tilt, Rainbow effect,
//          command parser, serial input
// ─────────────────────────────────────────────────────────────────

#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

#define LED_PIN       8
#define LED_COUNT     1
#define SERVO_PAN_PIN 2
#define SERVO_TIL_PIN 3

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
Servo servoPan;
Servo servoTilt;

uint8_t curR = 0, curG = 0, curB = 0, curW = 0;
int curPan = 90, curTilt = 90;

bool rainbowActive = false;
uint8_t rainbowHue = 0;
unsigned long lastRainbowStep = 0;
static uint8_t _preRainbowR = 0, _preRainbowG = 0, _preRainbowB = 0, _preRainbowW = 0;

// ── Hardware output ───────────────────────────────────────────────

void setLED(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  // Skip show() if nothing changed — NeoPixel show() briefly disables
  // interrupts; calling it 44×/s at identical values wastes CPU and adds jitter.
  if (r==curR && g==curG && b==curB && w==curW) return;
  strip.setPixelColor(0, strip.Color(r, g, b, w));
  strip.show();
  curR=r; curG=g; curB=b; curW=w;
}

// ── Servo smoothing ───────────────────────────────────────────────
// setPan / setTilt write to a TARGET; core_loop moves toward it at a
// fixed slew rate (200°/s).  This decouples servo motion from ArtNet
// packet timing jitter — irregular UDP spacing no longer causes jerks.
static float _tgtPan  = 90.0f, _tgtTilt  = 90.0f;
static float _curPanF = 90.0f, _curTiltF = 90.0f;

void setPan(int a)  { _tgtPan  = constrain(a, 0, 270); curPan  = a; }
void setTilt(int a) { _tgtTilt = constrain(a, 0, 270); curTilt = a; }

void hueToRGB(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t s=hue/43, o=(hue%43)*6;
  switch(s){
    case 0: r=255;   g=o;     b=0;     break;
    case 1: r=255-o; g=255;   b=0;     break;
    case 2: r=0;     g=255;   b=o;     break;
    case 3: r=0;     g=255-o; b=255;   break;
    case 4: r=o;     g=0;     b=255;   break;
    default:r=255;   g=0;     b=255-o; break;
  }
}

// ── Command parser ────────────────────────────────────────────────

void applyCommand(const String& cmd) {
  if (cmd.startsWith("RAINBOW:")) {
    bool newState = (cmd.substring(8).toInt() == 1);
    if (newState && !rainbowActive) {
      // Save current color before starting rainbow
      _preRainbowR = curR; _preRainbowG = curG; _preRainbowB = curB; _preRainbowW = curW;
    }
    rainbowActive = newState;
    rainbowHue = 0;
    if (!rainbowActive) setLED(_preRainbowR, _preRainbowG, _preRainbowB, _preRainbowW);
    return;
  }

  int r=curR,g=curG,b=curB,w=curW,pan=curPan,tilt=curTilt,pos=0;
  bool hasColor = false;   // true if the command contains any of R / G / B / W
  while (pos < (int)cmd.length()) {
    int comma = cmd.indexOf(',', pos);
    if (comma<0) comma = cmd.length();
    String tok = cmd.substring(pos, comma);
    int col = tok.indexOf(':');
    if (col>=0) {
      String key = tok.substring(0,col); key.toUpperCase();
      int val = tok.substring(col+1).toInt();
      if      (key=="R")    { r    = constrain(val,0,255); hasColor = true; }
      else if (key=="G")    { g    = constrain(val,0,255); hasColor = true; }
      else if (key=="B")    { b    = constrain(val,0,255); hasColor = true; }
      else if (key=="W")    { w    = constrain(val,0,255); hasColor = true; }
      else if (key=="PAN")  pan  = constrain(val,0,270);
      else if (key=="TILT") tilt = constrain(val,0,270);
    }
    pos = comma+1;
  }
  // Only kill rainbow and update LED when the command explicitly sets a color channel.
  // A pure PAN/TILT command must not touch rainbow or the LED at all —
  // the rainbow loop writes directly to the strip without updating curR/G/B/W,
  // so calling setLED(curR,curG,curB,curW) here would write (0,0,0,0) and black out the LED.
  if (hasColor) {
    rainbowActive = false;
    setLED(r,g,b,w);
  }
  setPan(pan); setTilt(tilt);
}

// ── Module lifecycle ──────────────────────────────────────────────

void core_setup() {
  Serial.println("[MiniHead] " FIRMWARE_VERSION);
  storage_begin();   // mount LittleFS — must run before any plugin setup()
  strip.begin(); strip.setBrightness(255); setLED(0,0,0,0);

  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  servoPan.setPeriodHertz(50);  servoPan.attach(SERVO_PAN_PIN, 500, 2500);
  servoTilt.setPeriodHertz(50); servoTilt.attach(SERVO_TIL_PIN, 500, 2500);
  _tgtPan = _curPanF = 135.0f;
  _tgtTilt = _curTiltF = 135.0f;
  setPan(135); setTilt(135);

  Serial.println("[Core] LED + Servos ready");
}

void core_loop() {
  // Serial command input
  if (Serial.available()) {
    static String buf = "";
    char c = Serial.read();
    if (c == '\n') {
      buf.trim();
      if (buf.length() > 0) { applyCommand(buf); Serial.println("[OK] " + buf); }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }

  // Servo smoothing — runs at 50 Hz, slew rate 200°/s (4°/step)
  // Decouples motion from ArtNet packet jitter.
  static unsigned long _lastServoMs = 0;
  {
    unsigned long now = millis();
    if (now - _lastServoMs >= 20) {
      float dt = (now - _lastServoMs) / 1000.0f;
      _lastServoMs = now;
      const float SLEW = 200.0f;   // °/s — raise for snappier, lower for smoother
      float maxStep = SLEW * dt;
      _curPanF  += constrain(_tgtPan  - _curPanF,  -maxStep, maxStep);
      _curTiltF += constrain(_tgtTilt - _curTiltF, -maxStep, maxStep);
      servoPan.writeMicroseconds(map((int)_curPanF,  0, 270, 500, 2500));
      servoTilt.writeMicroseconds(map((int)_curTiltF, 0, 270, 500, 2500));
    }
  }

  // Rainbow effect
  if (rainbowActive) {
    unsigned long now = millis();
    if (now - lastRainbowStep >= 20) {
      lastRainbowStep = now;
      uint8_t r,g,b; hueToRGB(rainbowHue, r, g, b);
      strip.setPixelColor(0, strip.Color(r,g,b,0)); strip.show();
      rainbowHue++;
    }
  }
}
