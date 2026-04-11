/*
 * =====================================================
 *   NANO MOVING LIGHT — WiFi Web Server Edition v2
 *   Hardware : ESP32-C3 Super Mini
 *   LED      : WS2812B RGBW (1x Pixel, GPIO8)
 *   Servos   : SG90 Pan (GPIO2), SG90 Tilt (GPIO3)
 *
 *   SETUP:
 *   1. Put BOTH files in the same sketch folder:
 *        nano_moving_light_wifi.ino
 *        html_page.h
 *   2. Fill in your WiFi credentials below
 *   3. Tools > Partition Scheme > Huge APP (3MB No OTA)
 *   4. Upload — open Serial Monitor at 115200 for IP
 *
 *   Libraries (Library Manager):
 *     - Adafruit NeoPixel
 *     - ESP32Servo
 *     - ArduinoJson  (by Benoit Blanchon, v7)
 * =====================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "html_page.h"        // HTML UI lives in this separate file

// ── !! CHANGE THESE !! ────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// ─────────────────────────────────────────────────────────────────

#define LED_PIN        8
#define LED_COUNT      1
#define SERVO_PAN_PIN  2
#define SERVO_TIL_PIN  3
#define MAX_CUES       32

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
Servo servoPan;
Servo servoTilt;
WebServer server(80);
Preferences prefs;

uint8_t curR = 0, curG = 0, curB = 0, curW = 0;
int curPan = 90, curTilt = 90;

bool rainbowActive = false;
uint8_t rainbowHue = 0;
unsigned long lastRainbowStep = 0;

struct Cue {
  unsigned long id;
  char name[32];
  uint8_t r, g, b, w;
  int pan, tilt;
};
Cue cues[MAX_CUES];
int cueCount = 0;

bool seqRunning = false;
unsigned long seqInterval = 1000;
bool seqLoop = true;
unsigned long seqIds[MAX_CUES];
int seqIdCount = 0;
int seqIndex = 0;
unsigned long lastSeqStep = 0;

// ── Hardware ──────────────────────────────────────────────────────

void setLED(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  strip.setPixelColor(0, strip.Color(r, g, b, w));
  strip.show();
  curR=r; curG=g; curB=b; curW=w;
}

void setPan(int a)  { servoPan.write(constrain(a,0,180));  curPan=constrain(a,0,180); }
void setTilt(int a) { servoTilt.write(constrain(a,0,180)); curTilt=constrain(a,0,180); }

void hueToRGB(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t s=hue/43, o=(hue%43)*6;
  switch(s){
    case 0: r=255;   g=o;     b=0;     break;
    case 1: r=255-o; g=255;   b=0;     break;
    case 2: r=0;     g=255;   b=o;     break;
    case 3: r=0;     g=255-o; b=255;   break;
    case 4: r=o;     g=0;     b=255;   break;
    default:r=255;   g=0;     b=255-o; break;
  }
}

// ── Flash storage ─────────────────────────────────────────────────

void saveCuesToFlash() {
  prefs.begin("cues", false);
  prefs.putInt("count", cueCount);
  for (int i=0; i<cueCount; i++)
    prefs.putBytes(("c"+String(i)).c_str(), &cues[i], sizeof(Cue));
  prefs.end();
}

void loadCuesFromFlash() {
  prefs.begin("cues", true);
  cueCount = prefs.getInt("count", 0);
  if (cueCount > MAX_CUES) cueCount = 0;
  for (int i=0; i<cueCount; i++)
    prefs.getBytes(("c"+String(i)).c_str(), &cues[i], sizeof(Cue));
  prefs.end();
}

// ── Helpers ───────────────────────────────────────────────────────

void sendJson(int code, const String& json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", json);
}

String cueToJson(const Cue& c) {
  return "{\"id\":" + String(c.id) +
    ",\"name\":\"" + String(c.name) + "\"" +
    ",\"r\":"   + c.r   + ",\"g\":" + c.g +
    ",\"b\":"   + c.b   + ",\"w\":" + c.w +
    ",\"pan\":" + c.pan + ",\"tilt\":" + c.tilt + "}";
}

void applyCommand(const String& cmd) {
  if (cmd.startsWith("RAINBOW:")) {
    rainbowActive = (cmd.substring(8).toInt() == 1);
    rainbowHue = 0;
    if (!rainbowActive) setLED(curR, curG, curB, curW);
    return;
  }
  rainbowActive = false;
  int r=curR,g=curG,b=curB,w=curW,pan=curPan,tilt=curTilt,pos=0;
  while (pos < (int)cmd.length()) {
    int comma = cmd.indexOf(',', pos);
    if (comma<0) comma = cmd.length();
    String tok = cmd.substring(pos, comma);
    int col = tok.indexOf(':');
    if (col>=0) {
      String key = tok.substring(0,col); key.toUpperCase();
      int val = tok.substring(col+1).toInt();
      if      (key=="R")    r    = constrain(val,0,255);
      else if (key=="G")    g    = constrain(val,0,255);
      else if (key=="B")    b    = constrain(val,0,255);
      else if (key=="W")    w    = constrain(val,0,255);
      else if (key=="PAN")  pan  = constrain(val,0,180);
      else if (key=="TILT") tilt = constrain(val,0,180);
    }
    pos = comma+1;
  }
  setLED(r,g,b,w); setPan(pan); setTilt(tilt);
}

// ── Route handlers ────────────────────────────────────────────────

void handleRoot()       { server.sendHeader("Access-Control-Allow-Origin","*"); server.send_P(200,"text/html",INDEX_HTML); }
void handleStatus()     { sendJson(200,"{\"connected\":true,\"port\":\"WiFi\",\"ip\":\""+WiFi.localIP().toString()+"\"}"); }
void handlePorts()      { sendJson(200,"[{\"port\":\"WiFi\",\"description\":\"ESP32 @ "+WiFi.localIP().toString()+"\"}]"); }
void handleConnect()    { sendJson(200,"{\"status\":\"ok\",\"port\":\"WiFi\"}"); }
void handleDisconnect() { sendJson(200,"{\"status\":\"ok\"}"); }
void handleSeqStop()    { seqRunning=false; sendJson(200,"{\"status\":\"ok\"}"); }
void handleSeqStatus()  { sendJson(200,String("{\"running\":"+(seqRunning?String("true"):String("false"))+"}")); }

void handleSend() {
  if (!server.hasArg("plain")) { sendJson(400,"{\"status\":\"error\",\"message\":\"No body\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\",\"message\":\"Bad JSON\"}"); return; }
  String cmd = doc["command"] | "";
  cmd.trim();
  applyCommand(cmd);
  sendJson(200,"{\"status\":\"ok\",\"response\":\"OK\"}");
}

void handleGetCues() {
  String json="[";
  for (int i=0; i<cueCount; i++) { if(i>0) json+=","; json+=cueToJson(cues[i]); }
  json+="]";
  sendJson(200,json);
}

void handleSaveCue() {
  if (cueCount>=MAX_CUES) { sendJson(500,"{\"status\":\"error\",\"message\":\"Max cues\"}"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\"}"); return; }
  Cue& c = cues[cueCount];
  c.id = millis();
  strlcpy(c.name, doc["name"]|"Cue", sizeof(c.name));
  c.r    = constrain((int)doc["r"],    0,255);
  c.g    = constrain((int)doc["g"],    0,255);
  c.b    = constrain((int)doc["b"],    0,255);
  c.w    = constrain((int)doc["w"],    0,255);
  c.pan  = constrain((int)doc["pan"],  0,180);
  c.tilt = constrain((int)doc["tilt"], 0,180);
  cueCount++;
  saveCuesToFlash();
  sendJson(200,"{\"status\":\"ok\",\"cue\":"+cueToJson(c)+"}");
}

void handleDeleteCue() {
  String path=server.uri();
  unsigned long id=path.substring(path.lastIndexOf('/')+1).toInt();
  for (int i=0; i<cueCount; i++) {
    if (cues[i].id==id) {
      for (int j=i; j<cueCount-1; j++) cues[j]=cues[j+1];
      cueCount--; saveCuesToFlash();
      sendJson(200,"{\"status\":\"ok\"}"); return;
    }
  }
  sendJson(404,"{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleFireCue() {
  String path=server.uri();
  String trimmed=path.substring(0,path.lastIndexOf('/'));
  unsigned long id=trimmed.substring(trimmed.lastIndexOf('/')+1).toInt();
  for (int i=0; i<cueCount; i++) {
    if (cues[i].id==id) {
      rainbowActive=false;
      setLED(cues[i].r,cues[i].g,cues[i].b,cues[i].w);
      setPan(cues[i].pan); setTilt(cues[i].tilt);
      sendJson(200,"{\"status\":\"ok\",\"command\":\"fired\",\"response\":\"OK\"}"); return;
    }
  }
  sendJson(404,"{\"status\":\"error\",\"message\":\"Not found\"}");
}

void handleSeqStart() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) { sendJson(400,"{\"status\":\"error\"}"); return; }
  seqInterval = doc["interval_ms"]|1000;
  seqLoop     = doc["loop"]|true;
  JsonArray ids = doc["cue_ids"].as<JsonArray>();
  seqIdCount=0;
  for (JsonVariant v : ids) if(seqIdCount<MAX_CUES) seqIds[seqIdCount++]=v.as<unsigned long>();
  seqIndex=0; seqRunning=(seqIdCount>0); lastSeqStep=millis();
  sendJson(200,"{\"status\":\"ok\"}");
}

void setupRoutes() {
  server.on("/",                     HTTP_GET,  handleRoot);
  server.on("/api/status",           HTTP_GET,  handleStatus);
  server.on("/api/ports",            HTTP_GET,  handlePorts);
  server.on("/api/connect",          HTTP_POST, handleConnect);
  server.on("/api/disconnect",       HTTP_POST, handleDisconnect);
  server.on("/api/send",             HTTP_POST, handleSend);
  server.on("/api/cues",             HTTP_GET,  handleGetCues);
  server.on("/api/cues",             HTTP_POST, handleSaveCue);
  server.on("/api/sequencer/start",  HTTP_POST, handleSeqStart);
  server.on("/api/sequencer/stop",   HTTP_POST, handleSeqStop);
  server.on("/api/sequencer/status", HTTP_GET,  handleSeqStatus);
  server.onNotFound([](){
    String path=server.uri();
    if (path.startsWith("/api/cues/")) {
      if (path.endsWith("/fire") && server.method()==HTTP_POST) { handleFireCue(); return; }
      if (server.method()==HTTP_DELETE) { handleDeleteCue(); return; }
    }
    server.send(404,"text/plain","Not found");
  });
}

// ── Setup ─────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);

  strip.begin(); strip.setBrightness(255); setLED(0,0,0,0);

  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  servoPan.setPeriodHertz(50);  servoPan.attach(SERVO_PAN_PIN,500,2400);
  servoTilt.setPeriodHertz(50); servoTilt.attach(SERVO_TIL_PIN,500,2400);
  setPan(90); setTilt(90);

  loadCuesFromFlash();
  Serial.printf("[INFO] Loaded %d cues from flash\n", cueCount);

  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  bool ledOn=false;
  while (WiFi.status()!=WL_CONNECTED) {
    delay(300);
    setLED(ledOn?15:0,ledOn?15:0,ledOn?15:0,0); ledOn=!ledOn;
    Serial.print(".");
  }
  setLED(0,60,0,0); delay(500); setLED(0,0,0,0);
  Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
  Serial.println("[HTTP] Open: http://" + WiFi.localIP().toString());

  setupRoutes();
  server.begin();
  Serial.println("[HTTP] Server started");
}

// ── Loop ──────────────────────────────────────────────────────────

void loop() {
  server.handleClient();
  // Serial command input
  if (Serial.available()) {
    static String buf = "";
    char c = Serial.read();
    if (c == '\n') {
      buf.trim();
      if (buf.length() > 0) {
        applyCommand(buf);
        Serial.println("[OK] " + buf);
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }

  if (rainbowActive) {
    unsigned long now=millis();
    if (now-lastRainbowStep>=20) {
      lastRainbowStep=now;
      uint8_t r,g,b; hueToRGB(rainbowHue,r,g,b);
      strip.setPixelColor(0,strip.Color(r,g,b,0)); strip.show();
      rainbowHue++;
    }
  }

  if (seqRunning && seqIdCount>0) {
    unsigned long now=millis();
    if (now-lastSeqStep>=seqInterval) {
      lastSeqStep=now;
      unsigned long tid=seqIds[seqIndex];
      for (int i=0; i<cueCount; i++) {
        if (cues[i].id==tid) {
          rainbowActive=false;
          setLED(cues[i].r,cues[i].g,cues[i].b,cues[i].w);
          setPan(cues[i].pan); setTilt(cues[i].tilt); break;
        }
      }
      if (++seqIndex>=seqIdCount) { if(seqLoop) seqIndex=0; else seqRunning=false; }
    }
  }
}
