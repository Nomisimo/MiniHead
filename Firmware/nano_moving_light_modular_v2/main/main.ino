/*
 * =====================================================
 *   NANO MOVING LIGHT — Modular Edition
 *   Hardware : ESP32-C3 Super Mini
 * =====================================================
 * main.ino — never needs to be touched.
 * Add/remove plugins in config.h only.
 * =====================================================
 */

#include "config.h"

SET_LOOP_TASK_STACK_SIZE(16384);  // default is 8192, double it

void setup() {
  Serial.begin(115200);
  core_setup();

  // Connect WiFi first — before any plugin runs
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

  Serial.print("[WiFi] Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected: " + WiFi.localIP().toString());

  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].setup();
}

void loop() {
  core_loop();
  for (int i = 0; i < _pluginCount; i++)
    _plugins[i].loop();
}
