#pragma once

// ── Startup Animation Plugin ──────────────────────────────────────
// Runs once on boot: slow servo sweep (min→max→center) + RGB color test.
// Total duration ~10 s. Enable via #define PLUGIN_STARTUP_ANIMATION in config.h.
//
// Note: uses direct servo writes (not setPan/setTilt) because this runs in
// setup() before core_loop() starts. Smoothing state is synced at each step
// so core_loop() picks up cleanly from the final position.
// ─────────────────────────────────────────────────────────────────

// Instantly snap to position and hold.
static void _sa_moveTo(int pan, int tilt, int holdMs) {
  pan  = constrain(pan,  0, 270);
  tilt = constrain(tilt, 0, 270);
  servoPan.writeMicroseconds(map(pan,  0, 270, 500, 2500));
  servoTilt.writeMicroseconds(map(tilt, 0, 270, 500, 2500));
  _tgtPan  = _curPanF  = (float)pan;
  _tgtTilt = _curTiltF = (float)tilt;
  curPan = pan; curTilt = tilt;
  delay(holdMs);
}

// Smoothly interpolate from current position to target over durationMs.
static void _sa_sweepTo(int toPan, int toTilt, int durationMs) {
  const int stepMs = 20;                    // ~50 Hz update rate
  int steps        = durationMs / stepMs;
  float fromPan    = _curPanF;
  float fromTilt   = _curTiltF;

  for (int i = 1; i <= steps; i++) {
    float t   = (float)i / steps;
    int   pan  = constrain((int)(fromPan  + t * (toPan  - fromPan)),  0, 270);
    int   tilt = constrain((int)(fromTilt + t * (toTilt - fromTilt)), 0, 270);
    servoPan.writeMicroseconds(map(pan,  0, 270, 500, 2500));
    servoTilt.writeMicroseconds(map(tilt, 0, 270, 500, 2500));
    _tgtPan  = _curPanF  = (float)pan;
    _tgtTilt = _curTiltF = (float)tilt;
    curPan = pan; curTilt = tilt;
    delay(stepMs);
  }
}

void startup_animation_setup() {
  Serial.println("[Startup] Calibration sweep...");

  // ── Servo sweep: min → max → center (~5.3 s) ─────────────────────
  _sa_moveTo(  0,   0,  300);   // snap to corner
  _sa_sweepTo(270, 270, 3000);  // slow sweep to opposite corner
  _sa_sweepTo(135, 135, 2000);  // slow return to center

  // ── Color fade: R→G→B→off (~4.6 s) ──────────────────────────────
  for (int i = 0; i < 256; i++) { setLED(255-i, i,     0,     0); delay(6); }
  for (int i = 0; i < 256; i++) { setLED(0,     255-i, i,     0); delay(6); }
  for (int i = 0; i < 256; i++) { setLED(i,     0,     255-i, 0); delay(6); }
  setLED(0, 0, 0, 0);

  Serial.println("[Startup] Done");
}

void startup_animation_loop() {}

REGISTER_PLUGIN(startup_animation);
