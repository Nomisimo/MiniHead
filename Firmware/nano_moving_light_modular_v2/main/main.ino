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
  delay(100);

  Serial.printf("[WiFi] SSID: \"%s\"\n", WIFI_SSID);
  Serial.print("[WiFi] Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - wifiStart > 15000) {
      Serial.println("\n[WiFi] FAILED — status: " + String(WiFi.status()));
      Serial.println("[WiFi] 0=idle 1=no SSID 2=scan done 3=connected");
      Serial.println("[WiFi] 4=connect fail 5=conn lost 6=disconnected");
      Serial.println("[WiFi] Check SSID/password and that router is 2.4GHz WPA2.");
      Serial.println("[WiFi] Retrying in 5s...");
      delay(5000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      wifiStart = millis();
    }
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
