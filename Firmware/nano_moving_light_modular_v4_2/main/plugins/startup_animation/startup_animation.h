#pragma once

// ── Startup Animation Plugin ──────────────────────────────────────
// Runs once on boot: servo calibration sweep (min→max→center) + RGB color test.
// Enable via #define PLUGIN_STARTUP_ANIMATION in config.h.
//
// Note: uses direct servoPan/servoTilt writes (not setPan/setTilt) because
// this runs in setup() before core_loop() starts — the exponential smoothing
// system hasn't ticked yet, so setPan() alone would never move the servos.
// The smoothing state variables are synced at the end so core_loop picks up
// cleanly from the final position.
//
// Depends on: core.h (setLED, servoPan, servoTilt, _tgtPan/_tiltF etc.)
// ─────────────────────────────────────────────────────────────────

static void _sa_moveTo(int pan, int tilt, int holdMs) {
  pan  = constrain(pan,  0, 270);
  tilt = constrain(tilt, 0, 270);
  servoPan.writeMicroseconds(map(pan,  0, 270, 500, 2500));
  servoTilt.writeMicroseconds(map(tilt, 0, 270, 500, 2500));
  // Sync smoothing state so core_loop() starts from the final position
  _tgtPan  = _curPanF  = (float)pan;
  _tgtTilt = _curTiltF = (float)tilt;
  curPan = pan; curTilt = tilt;
  delay(holdMs);
}

void startup_animation_setup() {
  Serial.println("[Startup] Calibration sweep...");

  // ── Servo calibration: min → max → center ────────────────────────
  // Mirrors what real moving heads do on power-up to find their range.
  _sa_moveTo(  0,   0, 600);   // pan left,  tilt down
  _sa_moveTo(270, 270, 600);   // pan right, tilt up
  _sa_moveTo(135, 135, 500);   // back to center

  // ── Color test: R → G → B → off ──────────────────────────────────
  for (int i = 0; i < 256; i++) { setLED(255-i, i,     0,     0); delay(5); }
  for (int i = 0; i < 256; i++) { setLED(0,     255-i, i,     0); delay(5); }
  for (int i = 0; i < 256; i++) { setLED(i,     0,     255-i, 0); delay(5); }
  setLED(0, 0, 0, 0);

  Serial.println("[Startup] Done");
}

void startup_animation_loop() {
  // Runs once in setup() only — nothing to do in loop
}

REGISTER_PLUGIN(startup_animation);
