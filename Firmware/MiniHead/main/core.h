#pragma once
#define FIRMWARE_VERSION       "Modular v4.2 — LittleFS + AsyncUDP"
#define FIRMWARE_VERSION_SHORT "4.2"
#include "plugin_registry.h"
#include "plugins/storage/storage.h"

// ── Core Module ───────────────────────────────────────────────────
// Handles: RGBW LED output, Servo Pan/Tilt, Rainbow/Demo effects,
//          command parser, serial input
// ─────────────────────────────────────────────────────────────────

#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <math.h>

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
static uint8_t _preRainbowR = 0, _preRainbowG = 0, _preRainbowB = 0, _preRainbowW = 0;
static float _rainbowHueF = 0.0f;  // float accumulator so animSpeed < 1.0 works

// ── Standalone animations ─────────────────────────────────────────
// Demo: sinusoidal pan+tilt+color — port of artnet_test.py demo_tick()
// Speed: shared rate multiplier for rainbow and demo (0.1 = slow, 3.0 = fast)
bool  demoActive = false;
float demoT      = 0.0f;   // elapsed seconds — drives all sin() math
float animSpeed  = 1.0f;   // 0.1–3.0; scales advance rate of all animations

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
// setPan / setTilt write to a TARGET; core_loop exponentially smooths
// toward it at 50 Hz.  Same technique as professional fixtures:
//   next = current + SMOOTH × (target − current)
// Natural ease-out: fast when far, slow on approach. Also absorbs
// ArtNet packet timing jitter — irregular UDP gaps no longer cause jerks.
// Tune SERVO_SMOOTH: 0.05 = very slow/silky, 0.3 = snappy.
// SERVO_DEADBAND: stop smoothing once within this many degrees of target.
// Prevents the motor hunting/oscillating around the target position.
#define SERVO_SMOOTH    0.12f
#define SERVO_DEADBAND  1.5f
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
      _preRainbowR = curR; _preRainbowG = curG; _preRainbowB = curB; _preRainbowW = curW;
    }
    demoActive    = false;          // rainbow and demo are mutually exclusive
    rainbowActive = newState;
    rainbowHue = 0; _rainbowHueF = 0.0f;
    if (!rainbowActive) setLED(_preRainbowR, _preRainbowG, _preRainbowB, _preRainbowW);
    return;
  }

  if (cmd.startsWith("DEMO:")) {
    bool newState = (cmd.substring(5).toInt() == 1);
    rainbowActive = false;          // mutually exclusive with rainbow
    // Reset hue accumulators so rainbow always starts from 0 after demo
    rainbowHue = 0; _rainbowHueF = 0.0f;
    demoActive    = newState;
    if (!newState) demoT = 0.0f;    // reset phase so next start is fresh
    return;
  }

  if (cmd.startsWith("SPEED:")) {
    animSpeed = constrain(cmd.substring(6).toFloat(), 0.1f, 3.0f);
    return;
  }

  if (cmd == "BLACKOUT") {
    rainbowActive = false;
    demoActive    = false;
    demoT         = 0.0f;
    setLED(0, 0, 0, 0);            // LED off — servos keep their current position
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
    demoActive    = false;          // explicit color command overrides all animations
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
      _lastServoMs = now;
      // Only advance smoothing outside the deadband — prevents oscillation
      // around the target when the motor lacks torque to close the last gap.
      if (fabsf(_tgtPan  - _curPanF)  > SERVO_DEADBAND)
        _curPanF  += SERVO_SMOOTH * (_tgtPan  - _curPanF);
      if (fabsf(_tgtTilt - _curTiltF) > SERVO_DEADBAND)
        _curTiltF += SERVO_SMOOTH * (_tgtTilt - _curTiltF);
      // Only write PWM when position actually changed — avoids constant
      // micro-corrections that keep the motor energised and hot.
      static int _lastPanUs = -1, _lastTiltUs = -1;
      int panUs  = map((int)_curPanF,  0, 270, 500, 2500);
      int tiltUs = map((int)_curTiltF, 0, 270, 500, 2500);
      if (panUs  != _lastPanUs)  { servoPan.writeMicroseconds(panUs);   _lastPanUs  = panUs;  }
      if (tiltUs != _lastTiltUs) { servoTilt.writeMicroseconds(tiltUs); _lastTiltUs = tiltUs; }
    }
  }

  // Rainbow effect (color only, no motion)
  static unsigned long lastRainbowStep = 0;
  if (rainbowActive) {
    unsigned long now = millis();
    if (now - lastRainbowStep >= 20) {
      lastRainbowStep = now;
      _rainbowHueF += animSpeed;           // float accumulator handles speed < 1
      rainbowHue = (uint8_t)_rainbowHueF;
      uint8_t r,g,b; hueToRGB(rainbowHue, r, g, b);
      strip.setPixelColor(0, strip.Color(r,g,b,0)); strip.show();
    }
  }

  // Demo effect — sinusoidal color + pan/tilt (port of artnet_test.py demo_tick)
  if (demoActive) {
    unsigned long now = millis();
    if (now - lastRainbowStep >= 20) {     // shares the same 20ms timer
      lastRainbowStep = now;
      demoT += 0.02f * animSpeed;          // advance elapsed time at speed-scaled rate
      uint8_t r, g, b;
      hueToRGB((uint8_t)(demoT * 42.67f), r, g, b);
      setLED(r, g, b, 0);
      float panDmx  = 127.0f + 127.0f * sinf(demoT * 0.5f);
      float tiltDmx = 127.0f + 100.0f * sinf(demoT * 0.3f + 1.0f);
      setPan( (int)(panDmx  / 255.0f * 270.0f));
      setTilt((int)(tiltDmx / 255.0f * 270.0f));
    }
  }
}
