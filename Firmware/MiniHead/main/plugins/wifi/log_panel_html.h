#pragma once

// ── Log Config Panel HTML Fragment ───────────────────────────────
// Served at GET /plugins/wifi/log_panel.html
// Injected into the module slot by the main page JS.
// Matches PC App log_panel.html visual style exactly.
// ─────────────────────────────────────────────────────────────────

const char LOG_PANEL_HTML[] PROGMEM = R"=====(
<style>
  .an-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:14px;padding-bottom:10px;border-bottom:1px solid var(--border);}
  .net-btn{padding:6px 12px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:11px;cursor:pointer;border-radius:4px;letter-spacing:1px;transition:all 0.15s;text-transform:uppercase;touch-action:manipulation;}
  .net-btn:hover{border-color:var(--accent);color:var(--accent);}
  .net-btn.primary{border-color:var(--accent);color:var(--accent);background:rgba(0,229,255,0.08);}

  /* ── Section label ── */
  .lc-section{font-family:var(--mono);font-size:9px;color:var(--text-dim);letter-spacing:2px;text-transform:uppercase;margin:14px 0 6px 0;padding-bottom:4px;border-bottom:1px solid rgba(42,42,58,0.7);}
  .lc-section:first-of-type{margin-top:0;}

  /* ── Toggle row ── */
  .lc-row{display:flex;align-items:center;gap:10px;padding:5px 0;border-bottom:1px solid rgba(42,42,58,0.4);}
  .lc-row:last-child{border-bottom:none;}
  .lc-name{font-family:var(--mono);font-size:11px;color:var(--text);font-weight:600;min-width:120px;}
  .lc-desc{font-family:var(--mono);font-size:10px;color:var(--text-dim);flex:1;}

  /* ── CSS toggle switch ── */
  .lc-sw{position:relative;display:inline-block;width:34px;height:18px;flex-shrink:0;}
  .lc-sw input{opacity:0;width:0;height:0;position:absolute;}
  .lc-sw .slider{position:absolute;cursor:pointer;inset:0;background:var(--surface2);border:1px solid var(--border);border-radius:18px;transition:background 0.15s,border-color 0.15s;}
  .lc-sw .slider:before{content:'';position:absolute;width:10px;height:10px;left:3px;top:50%;transform:translateY(-50%);background:var(--text-dim);border-radius:50%;transition:transform 0.15s,background 0.15s;}
  .lc-sw input:checked + .slider{background:rgba(0,229,255,0.15);border-color:var(--accent);}
  .lc-sw input:checked + .slider:before{transform:translate(16px,-50%);background:var(--accent);}

  /* ── Toast ── */
  #lc_toast{position:fixed;bottom:18px;right:18px;padding:6px 14px;background:var(--surface2);border:1px solid var(--accent);color:var(--accent);font-family:var(--mono);font-size:11px;border-radius:4px;letter-spacing:1px;opacity:0;pointer-events:none;transition:opacity 0.2s;z-index:9999;}
  #lc_toast.show{opacity:1;}
</style>

<div class="an-title">// Log Config</div>

<div class="lc-section">ART-NET</div>

<div class="lc-row">
  <label class="lc-sw">
    <input type="checkbox" id="lc_artnetFrames" onchange="lc_save()">
    <span class="slider"></span>
  </label>
  <span class="lc-name">artnetFrames</span>
  <span class="lc-desc">R/G/B/W/Pan/Tilt per packet &mdash; very spammy</span>
</div>
<div class="lc-row">
  <label class="lc-sw">
    <input type="checkbox" id="lc_artnetEvents" onchange="lc_save()">
    <span class="slider"></span>
  </label>
  <span class="lc-name">artnetEvents</span>
  <span class="lc-desc">active / timeout / patch changed</span>
</div>

<div class="lc-section">DISCOVERY</div>

<div class="lc-row">
  <label class="lc-sw">
    <input type="checkbox" id="lc_discoveryBeacons" onchange="lc_save()">
    <span class="slider"></span>
  </label>
  <span class="lc-name">discoveryBeacons</span>
  <span class="lc-desc">every beacon heard &mdash; spammy</span>
</div>
<div class="lc-row">
  <label class="lc-sw">
    <input type="checkbox" id="lc_discoveryEvents" onchange="lc_save()">
    <span class="slider"></span>
  </label>
  <span class="lc-name">discoveryEvents</span>
  <span class="lc-desc">role change, leader found/lost</span>
</div>

<div class="lc-section">UDP / COMMANDS</div>

<div class="lc-row">
  <label class="lc-sw">
    <input type="checkbox" id="lc_udpVerbose" onchange="lc_save()">
    <span class="slider"></span>
  </label>
  <span class="lc-name">udpVerbose</span>
  <span class="lc-desc">identify / relay / fallthrough commands</span>
</div>

<div id="lc_toast">SAVED</div>

<script>
(function(){
  // ── Load from ESP ────────────────────────────────────────────
  function lc_load() {
    fetch('/api/logconfig')
      .then(function(r){ return r.json(); })
      .then(function(d){
        lc_set('lc_artnetFrames',     d.artnetFrames);
        lc_set('lc_artnetEvents',     d.artnetEvents);
        lc_set('lc_discoveryBeacons', d.discoveryBeacons);
        lc_set('lc_discoveryEvents',  d.discoveryEvents);
        lc_set('lc_udpVerbose',       d.udpVerbose);
      })
      .catch(function(){ /* silent */ });
  }

  function lc_set(id, val) {
    var el = document.getElementById(id);
    if(el) el.checked = !!val;
  }

  // ── Auto-save on any toggle change ──────────────────────────
  window.lc_save = function() {
    var body = JSON.stringify({
      artnetFrames:     document.getElementById('lc_artnetFrames').checked,
      artnetEvents:     document.getElementById('lc_artnetEvents').checked,
      discoveryBeacons: document.getElementById('lc_discoveryBeacons').checked,
      discoveryEvents:  document.getElementById('lc_discoveryEvents').checked,
      udpVerbose:       document.getElementById('lc_udpVerbose').checked
    });
    fetch('/api/logconfig', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: body
    }).then(function(){
      lc_toast();
    }).catch(function(){ /* silent */ });
  };

  // ── Toast feedback ────────────────────────────────────────────
  var lc_toastTimer = null;
  function lc_toast() {
    var el = document.getElementById('lc_toast');
    if(!el) return;
    el.classList.add('show');
    if(lc_toastTimer) clearTimeout(lc_toastTimer);
    lc_toastTimer = setTimeout(function(){ el.classList.remove('show'); }, 1200);
  }

  // ── Boot ──────────────────────────────────────────────────────
  lc_load();
})();
</script>
)=====";
