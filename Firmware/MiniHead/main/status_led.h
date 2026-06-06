#pragma once

// ── Boot-phase status LED ─────────────────────────────────────────
// LED indicators run ONLY during setup(). Once the startup animation
// completes and loop() is running, the LED belongs to ArtNet/UDP.
//
// Color logic:   orange=system  violet=provisioning  blue=wifi
//                cyan=discovery  red=error/fallback
// Animation:     slow pulse (750ms) = waiting/listening
//                fast blink (150ms) = actively working
// ─────────────────────────────────────────────────────────────────

static inline bool _sled_slow(unsigned long ms) { return (ms / 750) % 2 == 0; }
static inline bool _sled_fast(unsigned long ms) { return (ms / 150) % 2 == 0; }

static void _sled_tripleFlash(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < 3; i++) {
    setLED(r, g, b, 0); delay(120);
    setLED(0, 0, 0, 0); delay(80);
  }
}

// ── One-shot states ───────────────────────────────────────────────

// Hardware init — orange solid (brief, just a heartbeat to show it's alive)
static inline void sled_bootInit()     { setLED(60, 20, 0, 0); }

// Credentials received via provisioning — triple violet flash, then reboot
static inline void sled_credReceived() { _sled_tripleFlash(40, 0, 80); }

// Role confirmed after election — triple cyan flash
static inline void sled_roleConfirmed() { _sled_tripleFlash(0, 50, 50); }

// AP fallback — triple red flash, then solid red
// Nothing overwrites it in AP mode (no ArtNet/UDP running), so it stays.
static inline void sled_apMode() {
  _sled_tripleFlash(80, 0, 0);
  setLED(50, 0, 0, 0);
}

// Boot complete — LED off, hand over to ArtNet/UDP
static inline void sled_bootDone()     { setLED(0, 0, 0, 0); }

// ── Tick calls (call every ~10–50 ms inside a boot-phase loop) ────

// Provisioning: BLE scanning for sender — violet slow pulse
static inline void sled_bleScan(unsigned long ms) {
  bool on = _sled_slow(ms);
  setLED(on?40:0, 0, on?80:0, 0);
}

// Provisioning: ESP-NOW channel hopping — violet fast blink
static inline void sled_espnowHop(unsigned long ms) {
  bool on = _sled_fast(ms);
  setLED(on?40:0, 0, on?80:0, 0);
}

// WiFi: connecting to SSID — blue slow pulse
static inline void sled_wifiConnect(unsigned long ms) {
  bool on = _sled_slow(ms);
  setLED(0, 0, on?80:0, 0);
}

// WiFi: all networks failed, waiting to retry — red slow pulse
static inline void sled_wifiRetry(unsigned long ms) {
  bool on = _sled_slow(ms);
  setLED(on?80:0, 0, 0, 0);
}

// Discovery: listening for peers (UDP beacons) — cyan slow pulse
static inline void sled_peerListen(unsigned long ms) {
  bool on = _sled_slow(ms);
  setLED(0, on?50:0, on?50:0, 0);
}

// Discovery: election in progress — cyan fast blink
static inline void sled_peerElection(unsigned long ms) {
  bool on = _sled_fast(ms);
  setLED(0, on?50:0, on?50:0, 0);
}
