#pragma once

// ── Log Config Panel HTML Fragment (Standalone v1) ────────────────
// Served at GET /plugins/log/panel.html

const char LOG_PANEL_HTML[] PROGMEM = R"=====(
<style>
  .an-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:14px;padding-bottom:10px;border-bottom:1px solid var(--border);}
  .lc-row{display:flex;align-items:center;gap:10px;padding:5px 0;}
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
<div class="lc-row">
  <label class="lc-sw">
    <input type="checkbox" id="lc_httpVerbose" onchange="lc_save()">
    <span class="slider"></span>
  </label>
  <span class="lc-name">httpVerbose</span>
  <span class="lc-desc">log every HTTP request to Serial</span>
</div>
<div id="lc_toast">SAVED</div>

<script>
(function(){
  function lc_load() {
    fetch('/api/logconfig').then(function(r){return r.json();}).then(function(d){
      var el=document.getElementById('lc_httpVerbose');if(el)el.checked=!!d.httpVerbose;
    }).catch(function(){});
  }
  window.lc_save = function() {
    var body=JSON.stringify({httpVerbose:document.getElementById('lc_httpVerbose').checked});
    fetch('/api/logconfig',{method:'POST',headers:{'Content-Type':'application/json'},body:body}).then(function(){
      var el=document.getElementById('lc_toast');if(!el)return;
      el.classList.add('show');setTimeout(function(){el.classList.remove('show');},1200);
    }).catch(function(){});
  };
  lc_load();
})();
</script>
)=====";
