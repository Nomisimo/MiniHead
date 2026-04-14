/*
 * =====================================================
 *   NANO MOVING LIGHT — Modular Edition  (v4)
 *   Hardware : ESP32-C3 Super Mini
 * =====================================================
 * main.ino — never needs to be touched.
 * Add/remove plugins in config.h only.
 *
 * WiFi is handled entirely by the wifi plugin:
 *   - Connects via NVS creds or config.h fallback
 *   - Falls back to AP captive portal if WiFi fails
 *   - No blocking loop here — plugins own their setup
 * =====================================================
 */

#include "config.h"

SET_LOOP_TASK_STACK_SIZE(16384);  // default is 8192, double it

void setup() {
  Serial.begin(115200);
  core_setup();

  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].setup();
}

void loop() {
  core_loop();
  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].loop();
}
