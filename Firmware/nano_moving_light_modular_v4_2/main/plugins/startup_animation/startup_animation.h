#pragma once

// ── Startup Animation Plugin ──────────────────────────────────────
// Runs once on boot: servo bounce + RGB color sweep
// Depends on: core.h (setLED, setPan, setTilt)
// ─────────────────────────────────────────────────────────────────

void startup_animation_setup() {
  // PAN bounce
  setPan(180);  delay(400);
  setPan(160);  delay(200);
  setPan(180);  delay(200);
  setPan(160);  delay(200);
  setPan(180);  delay(200);
  setPan(90);   delay(500);
  // TILT bounce
  setTilt(180); delay(400);
  setTilt(160); delay(200);
  setTilt(180); delay(200);
  setTilt(160); delay(200);
  setTilt(180); delay(200);
  setTilt(90);  delay(500);
  // Color sweep Red → Green → Blue → Red
  for (int i = 0; i < 256; i++) { setLED(255-i, i,     0,     0); delay(10); }
  for (int i = 0; i < 256; i++) { setLED(0,     255-i, i,     0); delay(10); }
  for (int i = 0; i < 256; i++) { setLED(i,     0,     255-i, 0); delay(10); }
  // LED off
  setLED(0, 0, 0, 0);
}

void startup_animation_loop() {
  // nothing — runs once in setup only
}

REGISTER_PLUGIN(startup_animation);
