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

  // Scan to verify SSID is visible
  Serial.println("[WiFi] Scanning...");
  int n = WiFi.scanNetworks();
  bool found = false;
  for (int i = 0; i < n; i++) {
    bool match = (WiFi.SSID(i) == String(WIFI_SSID));
    Serial.printf("  [%d] \"%s\"  RSSI:%d  Ch:%d  Enc:%d%s\n",
      i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
      WiFi.encryptionType(i), match ? "  <<< TARGET" : "");
    if (match) found = true;
  }
  if (!found) Serial.printf("[WiFi] WARNING: \"%s\" not found in scan!\n", WIFI_SSID);
  WiFi.scanDelete();

  Serial.printf("[WiFi] Connecting to \"%s\"\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - wifiStart > 15000) {
      Serial.println("\n[WiFi] FAILED — status: " + String(WiFi.status()));
      Serial.println("[WiFi] Retrying...");
      WiFi.disconnect(true);
      delay(500);
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
