#pragma once

// ── WiFi Manager ──────────────────────────────────────────────────
// Handles WiFi connection on boot:
//   1. Try saved NVS credentials (namespace "wificreds")
//   2. Fall back to WIFI_SSID / WIFI_PASSWORD from config.h
//   3. If both fail → start AP "MiniHead-XXXXXX" with captive portal
//
// Captive portal routes:
//   GET  /              → CAPTIVE_HTML (network picker + form)
//   GET  /wifi/scan     → JSON array of visible networks
//   POST /wifi/save     → save SSID+pass to NVS → reboot
//   GET  *              → 302 → http://192.168.4.1  (captive redirect)
//
// NOTE: wifi_manager is NOT a REGISTER_PLUGIN — it is called directly
// from wifi_setup() in wifi.h before discovery starts.
// ─────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "discovery_globals.h"
#include "wifi_captive_html.h"

#define WIFI_CONNECT_TIMEOUT_MS 15000

static WebServer _portalServer(80);

// ── Captive portal handlers ───────────────────────────────────────

static void handleCaptivePortal() {
  _portalServer.sendHeader("Cache-Control", "no-cache");
  _portalServer.send_P(200, "text/html", CAPTIVE_HTML);
}

static void handleWifiScan() {
  int n = WiFi.scanNetworks(false, true);  // blocking, include hidden
  String json = "[";
  for (int i = 0; i < n && i < 20; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) +
            ",\"enc\":" + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  json += "]";
  _portalServer.sendHeader("Access-Control-Allow-Origin", "*");
  _portalServer.send(200, "application/json", json);
}

static void handleWifiSave() {
  String ssid = _portalServer.arg("ssid");
  String pass = _portalServer.arg("pass");
  if (ssid.length() == 0) {
    _portalServer.send(400, "text/plain", "Missing SSID");
    return;
  }
  Preferences p; p.begin("wificreds", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
  Serial.printf("[WiFiMgr] Saved credentials for \"%s\" — restarting\n", ssid.c_str());
  _portalServer.send(200, "text/plain", "OK");
  delay(500);
  ESP.restart();
}

static void handleCaptiveRedirect() {
  _portalServer.sendHeader("Location", "http://192.168.4.1");
  _portalServer.send(302, "text/plain", "");
}

// ── AP mode start ─────────────────────────────────────────────────

static void startCaptivePortal() {
  nodeRole = ROLE_AP_PORTAL;

  uint8_t mac[6]; WiFi.macAddress(mac);
  char apName[24];
  snprintf(apName, sizeof(apName), "MiniHead-%02X%02X%02X", mac[3], mac[4], mac[5]);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName, "minihead");
  delay(100);

  Serial.printf("[WiFiMgr] AP started: \"%s\"  IP: %s\n",
                apName, WiFi.softAPIP().toString().c_str());

  _portalServer.on("/",           HTTP_GET,  handleCaptivePortal);
  _portalServer.on("/wifi/scan",  HTTP_GET,  handleWifiScan);
  _portalServer.on("/wifi/save",  HTTP_POST, handleWifiSave);
  _portalServer.onNotFound(handleCaptiveRedirect);
  _portalServer.begin();
}

// ── Main connect logic ────────────────────────────────────────────

void wifi_manager_setup() {
  // Load stored credentials; fall back to compile-time values
  Preferences p; p.begin("wificreds", true);
  String ssid = p.getString("ssid", WIFI_SSID);
  String pass = p.getString("pass", WIFI_PASSWORD);
  p.end();

  Serial.printf("[WiFiMgr] Connecting to \"%s\"\n", ssid.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);
  WiFi.begin(ssid.c_str(), pass.c_str());

  // Visual feedback: cyan blink during connect
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t > WIFI_CONNECT_TIMEOUT_MS) break;
    bool on = ((millis() / 300) % 2 == 0);
    setLED(0, on ? 20 : 0, on ? 20 : 0, 0);
    delay(50);
  }
  setLED(0, 0, 0, 0);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFiMgr] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    return;  // success — nodeRole stays ROLE_UNDECIDED until discovery_elect()
  }

  Serial.println("[WiFiMgr] Connection failed — starting captive portal AP");
  startCaptivePortal();
}

void wifi_manager_loop() {
  if (nodeRole == ROLE_AP_PORTAL) {
    _portalServer.handleClient();
  }
}
