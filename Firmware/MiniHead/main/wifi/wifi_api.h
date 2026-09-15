#pragma once

// ── General API handlers ──────────────────────────────────────────
// Included by wifi_control.h after wifi_helpers.h and server decl.
// Covers: root, status, version, ports, connect/disconnect,
//         send, rainbow, demo, animSpeed, blackout, AP password.
// ─────────────────────────────────────────────────────────────────

void handleRoot(AsyncWebServerRequest* req) {
#ifdef PLUGIN_ARTNET
  // Art-Net mode: redirect browsers to the PC App instead of serving the UI.
  // Use 127.0.0.1 when the browser is on the same machine as the Art-Net sender.
  String target = "http://127.0.0.1:8080";
  if (artnetSenderIP.length() > 0) {
    String clientIP = req->client()->remoteIP().toString();
    target = (clientIP == artnetSenderIP)
             ? "http://127.0.0.1:8080"
             : "http://" + artnetSenderIP + ":8080";
  }
  req->redirect(target);
  return;
#endif
  if (!requireLeader(req)) return;
  sendHtmlProgmem(req, INDEX_HTML);
}

void handleStatus(AsyncWebServerRequest* req) {
  String json = "{\"connected\":true,\"port\":\"WiFi\",\"ip\":\"" + _wifiIP() + "\""
              + ",\"apMode\":"        + (wifiAPMode    ? "true" : "false")
              + ",\"apPasswordSet\":" + (apPasswordSet ? "true" : "false")
              + ",\"rainbowActive\":" + (rainbowActive ? "true" : "false")
              + ",\"demoActive\":"    + (demoActive    ? "true" : "false")
              + ",\"animSpeed\":"     + String(animSpeed, 2)
              + "}";
  sendJson(req, 200, json);
}

void handleVersion(AsyncWebServerRequest* req)    { sendJson(req, 200, "{\"version\":\"" FIRMWARE_VERSION_SHORT "\"}"); }
void handlePorts(AsyncWebServerRequest* req)      { sendJson(req, 200, "[{\"port\":\"WiFi\",\"description\":\"ESP32 @ "+_wifiIP()+"\"}]"); }
void handleConnect(AsyncWebServerRequest* req)    { sendJson(req, 200, "{\"status\":\"ok\",\"port\":\"WiFi\"}"); }
void handleDisconnect(AsyncWebServerRequest* req) { sendJson(req, 200, "{\"status\":\"ok\"}"); }

void handleSend(AsyncWebServerRequest* req) {
  String body = _getBody(req);
  Serial.printf("[Send] body=%s\n", body.isEmpty() ? "<empty>" : body.c_str());
  if (body.isEmpty()) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"No body\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, body)) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String cmd = doc["command"] | "";
  cmd.trim();
  JsonArray targets = doc["targets"].as<JsonArray>();
  if (targets.isNull() || targets.size() == 0) {
    rainbowActive = false; demoActive = false;
    applyCommand(cmd);
  } else {
    for (JsonVariant t : targets) {
      String mac = t.as<String>();
      if (mac == "*") {
        rainbowActive = false; demoActive = false;
        applyCommand(cmd); udp_broadcastCommand(cmd.c_str()); break;
      } else if (mac == String(ownMAC) || mac == "self") {
        rainbowActive = false; demoActive = false; applyCommand(cmd);
      } else {
        for (int i = 0; i < peerCount; i++) {
          if (peers[i].active && String(peers[i].mac) == mac) {
            udp_sendCommand(peers[i].ip, peers[i].mac, cmd.c_str()); break;
          }
        }
      }
    }
  }
  sendJson(req, 200, "{\"status\":\"ok\",\"response\":\"OK\"}");
}

void handleRainbow(AsyncWebServerRequest* req) {
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  bool on = doc["on"] | false;
  String cmd = on ? "RAINBOW:1" : "RAINBOW:0";
  applyCommand(cmd);
  udp_broadcastCommand(cmd.c_str());
  Serial.printf("[WiFi] Rainbow global %s\n", on ? "ON" : "OFF");
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleDemo(AsyncWebServerRequest* req) {
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  bool on = doc["on"] | false;
  String cmd = on ? "DEMO:1" : "DEMO:0";
  applyCommand(cmd);
  udp_broadcastCommand(cmd.c_str());
  Serial.printf("[WiFi] Demo global %s\n", on ? "ON" : "OFF");
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleAnimSpeed(AsyncWebServerRequest* req) {
  JsonDocument doc;
  deserializeJson(doc, _getBody(req));
  float s = doc["speed"] | 1.0f;
  s = constrain(s, 0.1f, 3.0f);
  String cmd = "SPEED:" + String(s, 2);
  applyCommand(cmd);
  udp_broadcastCommand(cmd.c_str());
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleBlackout(AsyncWebServerRequest* req) {
  applyCommand("BLACKOUT");
  udp_broadcastCommand("BLACKOUT");
  Serial.println("[WiFi] Blackout");
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

void handleSetAPPassword(AsyncWebServerRequest* req) {
  if (!wifiAPMode) { sendJson(req, 403, "{\"status\":\"error\",\"message\":\"Not in AP mode\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, _getBody(req))) { sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String pw = doc["password"] | "";
  if (pw.length() > 0 && pw.length() < 8) {
    sendJson(req, 400, "{\"status\":\"error\",\"message\":\"Min. 8 characters (or empty to remove)\"}"); return;
  }
  JsonDocument cfg;
  storage_readJson("/config.json", cfg);
  cfg["apPassword"] = pw;
  storage_writeJson("/config.json", cfg);
  apPasswordSet = (pw.length() >= 8);
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "MiniHead-%02X%02X", deviceMAC[4], deviceMAC[5]);
  WiFi.softAPdisconnect(false);
  if (apPasswordSet) WiFi.softAP(ssid, pw.c_str());
  else               WiFi.softAP(ssid);
  Serial.printf("[WiFi] AP password %s\n", apPasswordSet ? "updated" : "cleared");
  sendJson(req, 200, String("{\"status\":\"ok\",\"reconnect\":") + (apPasswordSet ? "true" : "false") + "}");
}
