#pragma once

// ── Shared HTTP helpers ───────────────────────────────────────────
// Included by wifi_control.h after server + flag declarations.
// sendJson / sendHtmlProgmem / body accumulator / requireLeader /
// _jsonEscapeStr / _wifiIP
// ─────────────────────────────────────────────────────────────────

// ── Leader guard ──────────────────────────────────────────────────
// Smart redirect:
//   request from PC App machine → http://127.0.0.1:8080
//   request from any other device → http://<leaderIP>:8080
// Returns false when redirected; caller must return immediately.
static bool requireLeader(AsyncWebServerRequest* req) {
  if (_serverActive) return true;

  String leaderIP   = "";
  bool   leaderIsPC = false;
  for (int i = 0; i < peerCount; i++) {
    if (peers[i].active && peers[i].role == ROLE_LEADER) {
      leaderIP   = String(peers[i].ip);
      leaderIsPC = (peers[i].priority == 0);
      break;
    }
  }

  if (leaderIP.length() > 0) {
    if (leaderIsPC) {
      String clientIP = req->client()->remoteIP().toString();
      if (clientIP == leaderIP) req->redirect("http://127.0.0.1:8080");
      else                      req->redirect("http://" + leaderIP + ":8080");
    } else {
      req->redirect("http://" + leaderIP);
    }
    return false;
  }

  req->send(503, "application/json", "{\"status\":\"follower\",\"message\":\"Not the leader\"}");
  return false;
}

// ── Async body accumulation ───────────────────────────────────────
// ESPAsyncWebServer delivers POST/PUT bodies via a separate callback.
// Accumulate chunks into _tempObject, read once in the request handler.
static void _bodyAccumulator(AsyncWebServerRequest* req,
                              uint8_t* data, size_t len,
                              size_t index, size_t total) {
  if (index == 0) {
    req->_tempObject = new String();
    ((String*)req->_tempObject)->reserve(total > 0 ? total : 64);
  }
  if (req->_tempObject)
    ((String*)req->_tempObject)->concat((char*)data, len);
}

static String _getBody(AsyncWebServerRequest* req) {
  if (!req->_tempObject) return "";
  String body = *((String*)req->_tempObject);
  delete (String*)req->_tempObject;
  req->_tempObject = nullptr;
  return body;
}

// ── Response helpers ──────────────────────────────────────────────

void sendJson(AsyncWebServerRequest* req, int code, const String& json) {
  AsyncWebServerResponse* r = req->beginResponse(code, "application/json", json);
  r->addHeader("Access-Control-Allow-Origin", "*");
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
}

static void sendHtmlProgmem(AsyncWebServerRequest* req, const char* progmemStr) {
  // Always no-store: ESPAsyncWebServer doesn't send ETag/Last-Modified so
  // browsers mishandle conditional GETs on reload — panels stop loading.
  AsyncWebServerResponse* r = req->beginResponse_P(200, "text/html", progmemStr);
  r->addHeader("Access-Control-Allow-Origin", "*");
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
}

// ── JSON string escaping ──────────────────────────────────────────
static String _jsonEscapeStr(const char* s) {
  String out;
  out.reserve(strlen(s) + 4);
  for (const char* p = s; *p; p++) {
    if      (*p == '"')  out += "\\\"";
    else if (*p == '\\') out += "\\\\";
    else if (*p == '\n') out += "\\n";
    else if (*p == '\r') out += "\\r";
    else if (*p == '\t') out += "\\t";
    else                 out += *p;
  }
  return out;
}

// ── IP helper ─────────────────────────────────────────────────────
static String _wifiIP() {
  IPAddress ip = WiFi.localIP();
  return (ip != IPAddress(0,0,0,0)) ? ip.toString() : WiFi.softAPIP().toString();
}
