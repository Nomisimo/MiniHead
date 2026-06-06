// ══════════════════════════════════════════════════════════════════
//  DEBUGGER — Optional diagnostic plugin
//
//  Combines two tools:
//    1. Log Config  — toggle serial log verbosity at runtime via
//                     the web UI (/plugins/debugger/panel.html)
//                     and /api/logconfig
//    2. Profiler    — prints loop timing, heap, and FreeRTOS task
//                     stats to Serial every DEBUGGER_PROFILER_MS ms
//
//  Enable in config.h:
//    #define PLUGIN_DEBUGGER
//
//  IMPORTANT: Must be the LAST plugin in config.h so that
//             debugger_loop() measures the full loop iteration time.
// ══════════════════════════════════════════════════════════════════

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/task.h>

// ── Configuration ──────────────────────────────────────────────────
#define DEBUGGER_PROFILER_MS  5000  // profiler report interval
#define DEBUGGER_MAX_TASKS    20    // max FreeRTOS tasks in table
// ───────────────────────────────────────────────────────────────────

// ── Profiler state ─────────────────────────────────────────────────
static uint32_t _dbg_lastUs = 0;
static uint32_t _dbg_lastMs = 0;
static uint32_t _dbg_frames = 0;
static uint32_t _dbg_sumUs  = 0;

static const char _dbg_top[] PROGMEM =
  "\n══════════════"
  " [DEBUGGER] "
  "════════════════════";
static const char _dbg_bot[] PROGMEM =
  "══════════════"
  "══════════════"
  "══════════════════";

// ── Panel HTML ─────────────────────────────────────────────────────
static const char _DBG_PANEL_HTML[] PROGMEM = R"=====(
<style>
  .an-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:14px;padding-bottom:10px;border-bottom:1px solid var(--border);}
  .net-btn{padding:6px 12px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:11px;cursor:pointer;border-radius:4px;letter-spacing:1px;transition:all 0.15s;text-transform:uppercase;touch-action:manipulation;}
  .net-btn:hover{border-color:var(--accent);color:var(--accent);}
  .lc-section{font-family:var(--mono);font-size:9px;color:var(--text-dim);letter-spacing:2px;text-transform:uppercase;margin:14px 0 6px 0;padding-bottom:4px;border-bottom:1px solid rgba(42,42,58,0.7);}
  .lc-section:first-of-type{margin-top:0;}
  .lc-row{display:flex;align-items:center;gap:10px;padding:5px 0;border-bottom:1px solid rgba(42,42,58,0.4);}
  .lc-row:last-child{border-bottom:none;}
  .lc-name{font-family:var(--mono);font-size:11px;color:var(--text);font-weight:600;min-width:120px;}
  .lc-desc{font-family:var(--mono);font-size:10px;color:var(--text-dim);flex:1;}
  .lc-sw{position:relative;display:inline-block;width:34px;height:18px;flex-shrink:0;}
  .lc-sw input{opacity:0;width:0;height:0;position:absolute;}
  .lc-sw .slider{position:absolute;cursor:pointer;inset:0;background:var(--surface2);border:1px solid var(--border);border-radius:18px;transition:background 0.15s,border-color 0.15s;}
  .lc-sw .slider:before{content:'';position:absolute;width:10px;height:10px;left:3px;top:50%;transform:translateY(-50%);background:var(--text-dim);border-radius:50%;transition:transform 0.15s,background 0.15s;}
  .lc-sw input:checked + .slider{background:rgba(0,229,255,0.15);border-color:var(--accent);}
  .lc-sw input:checked + .slider:before{transform:translate(16px,-50%);background:var(--accent);}
  #lc_toast{position:fixed;bottom:18px;right:18px;padding:6px 14px;background:var(--surface2);border:1px solid var(--accent);color:var(--accent);font-family:var(--mono);font-size:11px;border-radius:4px;letter-spacing:1px;opacity:0;pointer-events:none;transition:opacity 0.2s;z-index:9999;}
  #lc_toast.show{opacity:1;}
</style>

<div class="an-title">// Log Config</div>

<div class="lc-section">ART-NET</div>
<div class="lc-row">
  <label class="lc-sw"><input type="checkbox" id="lc_artnetFrames" onchange="lc_save()"><span class="slider"></span></label>
  <span class="lc-name">artnetFrames</span>
  <span class="lc-desc">R/G/B/W/Pan/Tilt per packet &mdash; very spammy</span>
</div>
<div class="lc-row">
  <label class="lc-sw"><input type="checkbox" id="lc_artnetEvents" onchange="lc_save()"><span class="slider"></span></label>
  <span class="lc-name">artnetEvents</span>
  <span class="lc-desc">active / timeout / patch changed</span>
</div>

<div class="lc-section">DISCOVERY</div>
<div class="lc-row">
  <label class="lc-sw"><input type="checkbox" id="lc_discoveryBeacons" onchange="lc_save()"><span class="slider"></span></label>
  <span class="lc-name">discoveryBeacons</span>
  <span class="lc-desc">every beacon heard &mdash; spammy</span>
</div>
<div class="lc-row">
  <label class="lc-sw"><input type="checkbox" id="lc_discoveryEvents" onchange="lc_save()"><span class="slider"></span></label>
  <span class="lc-name">discoveryEvents</span>
  <span class="lc-desc">role change, leader found/lost</span>
</div>

<div class="lc-section">UDP / COMMANDS</div>
<div class="lc-row">
  <label class="lc-sw"><input type="checkbox" id="lc_udpVerbose" onchange="lc_save()"><span class="slider"></span></label>
  <span class="lc-name">udpVerbose</span>
  <span class="lc-desc">identify / relay / fallthrough commands</span>
</div>

<div id="lc_toast">SAVED</div>

<script>
(function(){
  function lc_load() {
    fetch('/api/logconfig').then(function(r){return r.json();}).then(function(d){
      ['artnetFrames','artnetEvents','discoveryBeacons','discoveryEvents','udpVerbose'].forEach(function(k){
        var el=document.getElementById('lc_'+k); if(el) el.checked=!!d[k];
      });
    }).catch(function(){});
  }
  window.lc_save=function(){
    var body=JSON.stringify({
      artnetFrames:    document.getElementById('lc_artnetFrames').checked,
      artnetEvents:    document.getElementById('lc_artnetEvents').checked,
      discoveryBeacons:document.getElementById('lc_discoveryBeacons').checked,
      discoveryEvents: document.getElementById('lc_discoveryEvents').checked,
      udpVerbose:      document.getElementById('lc_udpVerbose').checked
    });
    fetch('/api/logconfig',{method:'POST',headers:{'Content-Type':'application/json'},body:body})
      .then(function(){
        var el=document.getElementById('lc_toast');
        if(!el)return; el.classList.add('show');
        setTimeout(function(){el.classList.remove('show');},1200);
      }).catch(function(){});
  };
  lc_load();
})();
</script>
)=====";

// ── HTTP handlers ──────────────────────────────────────────────────

static void _dbg_handlePanel(AsyncWebServerRequest* req) {
  AsyncWebServerResponse* res = req->beginResponse_P(200, "text/html",
    (const uint8_t*)_DBG_PANEL_HTML, strlen_P(_DBG_PANEL_HTML));
  req->send(res);
}

static void _dbg_handleGetConfig(AsyncWebServerRequest* req) {
  sendJson(req, 200, logcfg_toJson());
}

static void _dbg_handleSetConfig(AsyncWebServerRequest* req) {
  logcfg_fromJson(_getBody(req));
  sendJson(req, 200, "{\"status\":\"ok\"}");
}

// ── Setup / Loop ───────────────────────────────────────────────────

void debugger_setup() {
  server.on("/plugins/debugger/panel.html", HTTP_GET,
    [](AsyncWebServerRequest* r){ _dbg_handlePanel(r); });
  server.on("/api/logconfig", HTTP_GET,
    [](AsyncWebServerRequest* r){ _dbg_handleGetConfig(r); });
  server.on("/api/logconfig", HTTP_POST,
    [](AsyncWebServerRequest* r){ _dbg_handleSetConfig(r); },
    nullptr, _bodyAccumulator);

  _dbg_lastUs = micros();
  _dbg_lastMs = millis();
  Serial.printf("[Debugger] started — profiler every %u ms\n", DEBUGGER_PROFILER_MS);
}

void debugger_loop() {
  uint32_t now = micros();
  _dbg_sumUs += now - _dbg_lastUs;
  _dbg_lastUs = now;
  _dbg_frames++;

  if (millis() - _dbg_lastMs < DEBUGGER_PROFILER_MS) return;
  _dbg_lastMs = millis();

  uint32_t avgUs  = _dbg_frames ? (_dbg_sumUs / _dbg_frames) : 0;
  uint32_t freqHz = avgUs       ? (1000000UL  / avgUs)       : 0;
  _dbg_frames = 0;
  _dbg_sumUs  = 0;

  Serial.println((__FlashStringHelper*)_dbg_top);
  Serial.printf("  Uptime        : %10lu ms\n", millis());
  Serial.printf("  Loop freq     : %10lu Hz    avg frame: %lu us\n",
                (unsigned long)freqHz, (unsigned long)avgUs);
  Serial.printf("  Free heap     : %10u B    min ever: %u B\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
  Serial.printf("  Max alloc blk : %10u B\n", ESP.getMaxAllocHeap());

  int rssi = WiFi.RSSI();
  if (rssi == 0 || WiFi.status() != WL_CONNECTED)
    Serial.println(F("  WiFi RSSI     :   -- (not connected)"));
  else
    Serial.printf("  WiFi RSSI     : %10d dBm\n", rssi);

  Serial.printf("  CPU freq      : %10u MHz\n", ESP.getCpuFreqMHz());

  TaskStatus_t tasks[DEBUGGER_MAX_TASKS];
  UBaseType_t taskCount = uxTaskGetSystemState(tasks, DEBUGGER_MAX_TASKS, NULL);
  Serial.println(F(""));
  Serial.println(F("  Tasks         Name             St  StackFree  Prio"));
  Serial.println(F("                ─────────────────────────────────────"));
  for (UBaseType_t i = 0; i < taskCount; i++) {
    char st = '?';
    switch (tasks[i].eCurrentState) {
      case eRunning:   st = 'R'; break;
      case eReady:     st = 'r'; break;
      case eBlocked:   st = 'B'; break;
      case eSuspended: st = 'S'; break;
      case eDeleted:   st = 'D'; break;
      default: break;
    }
    uint32_t stackFree = (uint32_t)tasks[i].usStackHighWaterMark * sizeof(StackType_t);
    Serial.printf("                %-16s %c  %9u  %4u%s\n",
                  tasks[i].pcTaskName, st, stackFree,
                  (uint32_t)tasks[i].uxCurrentPriority,
                  (stackFree < 512) ? " !!!" : "");
  }
  Serial.println((__FlashStringHelper*)_dbg_bot);
}

REGISTER_PLUGIN(debugger);
