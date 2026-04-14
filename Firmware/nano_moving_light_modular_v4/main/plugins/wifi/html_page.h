#pragma once

// ── Embedded HTML UI ─────────────────────────────────────────────
// Stored in flash (PROGMEM). Served by the ESP32 web server at GET /

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Nano Moving Light</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow:wght@300;400;600;700&display=swap" rel="stylesheet">
<style>
  :root {
    --bg:#0a0a0f;--surface:#13131a;--surface2:#1c1c26;
    --border:#2a2a3a;--accent:#00e5ff;--accent2:#ff6b35;
    --accent3:#a855f7;--text:#e0e0f0;--text-dim:#6b6b8a;
    --success:#22c55e;--danger:#ef4444;
    --mono:'Share Tech Mono',monospace;--sans:'Barlow',sans-serif;
  }
  *{margin:0;padding:0;box-sizing:border-box;}
  body{background:var(--bg);color:var(--text);font-family:var(--sans);min-height:100vh;overflow-x:hidden;}
  body::before{content:'';position:fixed;inset:0;background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,229,255,0.015) 2px,rgba(0,229,255,0.015) 4px);pointer-events:none;z-index:1000;}

  /* ── Header ── */
  header{display:flex;align-items:center;justify-content:space-between;padding:12px 16px;border-bottom:1px solid var(--border);background:var(--surface);position:sticky;top:0;z-index:100;}
  .logo{font-family:var(--mono);font-size:16px;color:var(--accent);letter-spacing:2px;text-transform:uppercase;}
  .logo span{color:var(--text-dim);}
  .connection-bar{display:flex;align-items:center;gap:8px;font-family:var(--mono);font-size:11px;}
  .status-dot{width:8px;height:8px;border-radius:50%;background:var(--success);box-shadow:0 0 8px var(--success);animation:pulse 2s infinite;flex-shrink:0;}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
  .ip-label{color:var(--accent);}

  /* ── Buttons ── */
  .btn{padding:8px 16px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:12px;cursor:pointer;border-radius:4px;letter-spacing:1px;transition:all 0.15s;text-transform:uppercase;touch-action:manipulation;}
  .btn:active{opacity:0.7;}
  .btn.primary{border-color:var(--accent);color:var(--accent);background:rgba(0,229,255,0.08);}
  .btn.danger{border-color:var(--danger);color:var(--danger);}
  .btn.success{border-color:var(--success);color:var(--success);}
  .btn.active{background:rgba(168,85,247,0.2);border-color:var(--accent3);color:var(--accent3);}

  /* ── Panel ── */
  .panel{background:var(--bg);padding:16px;border-bottom:1px solid var(--border);}
  .panel:last-child{border-bottom:none;}
  .panel-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:16px;padding-bottom:10px;border-bottom:1px solid var(--border);}

  /* ── MAIN GRID ── */
  .main{
    display:grid;
    grid-template-columns:1fr;
    grid-template-areas:
      "light"
      "motion"
      "rainbow"
      "serial"
      "cues"
      "sequencer"
      "future";
    gap:1px;
    background:var(--border);
  }
  @media(min-width:900px){
    .main{
      grid-template-columns:2fr 1fr;
      grid-template-rows:auto auto auto auto auto 1fr;
      grid-template-areas:
        "cues       light"
        "sequencer  motion"
        "future     rainbow"
        "future     serial"
        "future     artpatch"
        "future     .";
      min-height:calc(100vh - 49px);
    }
    .col-right{border-left:1px solid var(--border);}
  }

  .area-light     {grid-area:light;}
  .area-motion    {grid-area:motion;}
  .area-rainbow   {grid-area:rainbow;}
  .area-serial    {grid-area:serial;}
  .area-artpatch  {grid-area:artpatch;}
  .area-cues      {grid-area:cues;}
  .area-sequencer {grid-area:sequencer;}
  .area-future    {grid-area:future;}

  /* ── Art-Net patch sidebar ── */
  .ap-table{width:100%;border-collapse:collapse;font-family:var(--mono);font-size:11px;}
  .ap-table th{text-align:left;color:var(--text-dim);font-weight:normal;padding:3px 4px;border-bottom:1px solid var(--border);font-size:10px;letter-spacing:1px;}
  .ap-table td{padding:3px 4px;border-bottom:1px solid rgba(42,42,58,0.4);vertical-align:middle;}
  .ap-table tr:last-child td{border-bottom:none;}
  .ap-num{width:44px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:2px 3px;font-family:var(--mono);font-size:10px;border-radius:3px;text-align:center;-moz-appearance:textfield;}
  .ap-num::-webkit-outer-spin-button,.ap-num::-webkit-inner-spin-button{-webkit-appearance:none;}
  .ap-del{width:20px;height:20px;border:1px solid var(--border);background:transparent;color:var(--danger);border-radius:3px;cursor:pointer;font-size:10px;line-height:1;padding:0;}

  /* ── RGBW Faders ── */
  .faders-row{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;align-items:end;}
  .fader-group{display:flex;flex-direction:column;align-items:center;gap:8px;}
  .fader-label{font-family:var(--mono);font-size:10px;color:var(--text-dim);letter-spacing:2px;}
  .vslider-wrap{width:36px;height:140px;display:flex;align-items:center;justify-content:center;}
  input[type=range].vertical{writing-mode:vertical-lr;direction:rtl;width:36px;height:140px;-webkit-appearance:slider-vertical;appearance:slider-vertical;cursor:pointer;}
  input[type=range]{-webkit-appearance:none;appearance:none;background:transparent;cursor:pointer;}
  input[type=range].vertical::-webkit-slider-thumb{-webkit-appearance:none;width:32px;height:16px;border-radius:3px;background:var(--accent);box-shadow:0 0 12px var(--accent);border:2px solid var(--bg);}
  .fader-r input[type=range]::-webkit-slider-thumb{background:#ff4444;box-shadow:0 0 12px #ff4444;}
  .fader-g input[type=range]::-webkit-slider-thumb{background:#44ff44;box-shadow:0 0 12px #44ff44;}
  .fader-b input[type=range]::-webkit-slider-thumb{background:#4488ff;box-shadow:0 0 12px #4488ff;}
  .fader-w input[type=range]::-webkit-slider-thumb{background:#ffffff;box-shadow:0 0 12px #ffffff;}
  input[type=number]{width:44px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:4px 2px;font-family:var(--mono);font-size:14px;border-radius:4px;outline:none;text-align:center;}
  .preview-row{display:flex;justify-content:center;margin-top:16px;}
  .led-preview{width:64px;height:64px;border-radius:50%;border:2px solid var(--border);transition:background 0.1s,box-shadow 0.1s;background:#000;}

  /* ── Motion ── */
  .motion-row{display:grid;grid-template-columns:40px 1fr 52px;align-items:center;gap:10px;margin-bottom:14px;}
  .motion-label{font-family:var(--mono);font-size:11px;color:var(--text-dim);letter-spacing:2px;}
  input[type=range].horizontal{-webkit-appearance:none;appearance:none;width:100%;height:6px;background:var(--surface2);border-radius:3px;outline:none;cursor:pointer;}
  input[type=range].horizontal::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;border-radius:50%;background:var(--accent2);box-shadow:0 0 10px var(--accent2);border:2px solid var(--bg);}

  /* ── Rainbow ── */
  .rainbow-btn{width:100%;padding:14px;font-size:13px;letter-spacing:3px;background:linear-gradient(90deg,#ff000033,#ffff0033,#00ff0033,#00ffff33,#0000ff33,#ff00ff33);border:1px solid var(--border);color:var(--text);transition:all 0.3s;touch-action:manipulation;}
  .rainbow-btn.active{background:linear-gradient(90deg,#ff0000aa,#ffff00aa,#00ff00aa,#00ffffaa,#0000ffaa,#ff00ffaa);animation:rainbow-shift 2s linear infinite;color:#fff;border-color:transparent;}
  @keyframes rainbow-shift{0%{filter:hue-rotate(0deg)}100%{filter:hue-rotate(360deg)}}

  /* ── Cues ── */
  .cue-list{display:flex;flex-direction:column;gap:6px;margin-bottom:12px;max-height:300px;overflow-y:auto;}
  .cue-list::-webkit-scrollbar{width:4px;}
  .cue-list::-webkit-scrollbar-thumb{background:var(--border);border-radius:2px;}
  .cue-item{display:flex;align-items:center;gap:8px;padding:10px 12px;background:var(--surface2);border:1px solid var(--border);border-radius:4px;cursor:default;}
  .cue-item.seq-selected{border-color:var(--accent3);background:rgba(168,85,247,0.1);}
  .cue-item.drag-over{border-color:var(--accent);background:rgba(99,102,241,0.15);}
  .cue-item.dragging{opacity:0.4;}
  .cue-drag{cursor:grab;color:var(--text-dim);font-size:16px;padding:0 2px;flex-shrink:0;user-select:none;line-height:1;}
  .cue-swatch{width:28px;height:28px;border-radius:4px;flex-shrink:0;border:1px solid rgba(255,255,255,0.1);}
  .cue-info{flex:1;min-width:0;}
  .cue-name{font-size:13px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .cue-meta{font-family:var(--mono);font-size:10px;color:var(--text-dim);margin-top:2px;}
  .cue-actions{display:flex;gap:4px;}
  .icon-btn{width:32px;height:32px;border:1px solid var(--border);background:transparent;color:var(--text-dim);border-radius:3px;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;touch-action:manipulation;}
  .save-cue-form{display:flex;gap:8px;}
  input[type=text]{flex:1;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:8px 10px;font-family:var(--mono);font-size:13px;border-radius:4px;outline:none;}

  /* ── Sequencer ── */
  .seq-controls{display:flex;flex-direction:column;gap:10px;}
  .seq-row{display:flex;align-items:center;gap:8px;}
  .seq-label{font-family:var(--mono);font-size:10px;color:var(--text-dim);width:60px;flex-shrink:0;}
  .seq-buttons{display:flex;gap:8px;margin-top:10px;}
  .seq-buttons .btn{flex:1;padding:12px;text-align:center;}
  .seq-hint{font-size:11px;color:var(--text-dim);margin-bottom:10px;}

  /* ── Future placeholder ── */
  .future-placeholder{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;min-height:120px;border:1px dashed var(--border);border-radius:4px;color:var(--text-dim);font-family:var(--mono);font-size:11px;letter-spacing:2px;}

  /* ── Serial ── */
  .serial-row{display:flex;gap:8px;}

  /* ── Cue Edit Modal ── */
  #cueEditModal{position:fixed;inset:0;background:rgba(0,0,0,0.82);z-index:600;display:none;align-items:center;justify-content:center;}
  #cueEditModal.open{display:flex;}
  .modal-box{background:var(--surface);border:1px solid var(--border);border-radius:6px;padding:20px;min-width:260px;max-width:380px;width:90%;max-height:80vh;overflow-y:auto;}
  .modal-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:14px;padding-bottom:8px;border-bottom:1px solid var(--border);}
  .modal-check-row{display:flex;align-items:center;gap:8px;margin-bottom:7px;font-family:var(--mono);font-size:11px;cursor:pointer;}
  .modal-check-row input{accent-color:var(--accent3);width:15px;height:15px;flex-shrink:0;}
  .modal-footer{display:flex;gap:8px;margin-top:14px;}

  /* ── Toast ── */
  #toast{position:fixed;bottom:16px;right:16px;left:16px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:10px 18px;font-family:var(--mono);font-size:12px;border-radius:4px;opacity:0;transition:opacity 0.2s;z-index:999;pointer-events:none;text-align:center;}
  #toast.show{opacity:1;}
  #toast.ok{border-color:var(--success);color:var(--success);}
  #toast.err{border-color:var(--danger);color:var(--danger);}
  @media(min-width:900px){#toast{left:auto;width:auto;}}
  /* ── Art-Net bar ── */
  #artnet-bar{display:none;align-items:center;gap:8px;padding:5px 16px;background:rgba(34,197,94,0.12);border-bottom:1px solid var(--success);font-family:var(--mono);font-size:11px;color:var(--success);letter-spacing:1px;}
  #artnet-bar.visible{display:flex;}
  #artnet-bar-dot{width:8px;height:8px;border-radius:50%;background:var(--success);box-shadow:0 0 6px var(--success);animation:pulse 1s infinite;flex-shrink:0;}
  .artnet-locked{pointer-events:none!important;opacity:0.45;transition:opacity 0.3s;}
  .area-light,.area-motion,.area-rainbow,.area-serial,.area-sequencer{position:relative;}
</style>
</head>
<body>
<header>
  <div class="logo">NANO<span>/</span>ML <span style="font-size:10px;color:var(--text-dim)">CTRL</span></div>
  <div class="connection-bar">
    <div class="status-dot"></div>
    <span class="ip-label" id="ipLabel">...</span>
    <span style="color:var(--text-dim)" id="modeLabel">ESP32</span>
  </div>
</header>
<div id="artnet-bar">
  <div id="artnet-bar-dot"></div>
  <span>&#9679;&nbsp;Running on: Art-Net</span>
  <span id="artnet-bar-count" style="color:var(--text-dim);margin-left:auto;"></span>
</div>

<div class="main">

  <!-- ── LEFT 2/3 ── -->

  <!-- Cues -->
  <div class="panel area-cues">
    <div class="panel-title">// Cues</div>
    <div style="display:flex;gap:6px;margin-bottom:10px;">
      <button class="btn" onclick="exportCues()" style="padding:5px 10px;font-size:10px;">&#8595; EXPORT</button>
      <label class="btn" style="padding:5px 10px;font-size:10px;cursor:pointer;">&#8593; IMPORT<input type="file" accept=".json" style="display:none" onchange="importCues(this)"></label>
    </div>
    <div class="cue-list" id="cueList"></div>
    <div class="save-cue-form">
      <input type="text" id="cueName" placeholder="Cue name..." maxlength="30">
      <button class="btn primary" onclick="saveCue()">SAVE</button>
    </div>
  </div>

  <!-- Sequencer -->
  <div class="panel area-sequencer">
    <div class="panel-title">// Sequencer</div>
    <div class="seq-hint">Tap + on cues to add to sequence</div>
    <div class="seq-controls">
      <div class="seq-row">
        <div class="seq-label">INTERVAL</div>
        <input type="number" id="seqInterval" value="1000" min="100" max="30000" step="100" style="width:80px;">
        <span style="font-family:var(--mono);font-size:10px;color:var(--text-dim)">ms</span>
      </div>
      <div class="seq-row">
        <div class="seq-label">LOOP</div>
        <label style="display:flex;align-items:center;gap:6px;cursor:pointer;">
          <input type="checkbox" id="seqLoop" checked style="accent-color:var(--accent3);width:18px;height:18px;">
          <span style="font-family:var(--mono);font-size:11px;color:var(--text-dim)">Repeat</span>
        </label>
      </div>
    </div>
    <div class="seq-buttons">
      <button class="btn success" id="seqStartBtn" onclick="startSequencer()">START</button>
      <button class="btn danger" onclick="stopSequencer()">STOP</button>
    </div>
    <div style="margin-top:8px;font-family:var(--mono);font-size:10px;color:var(--text-dim)" id="seqStatus">Sequencer idle</div>
  </div>

  <!-- Modules: Network Heads + Art-Net Patch -->
  <div class="area-future" style="background:var(--bg)">
    <div class="panel" id="module-container" style="border-bottom:1px solid var(--border)">
      <div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:20px 0;">Loading...</div>
    </div>
    <div class="panel" id="artnet-module-container">
      <div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:20px 0;">Loading Art-Net...</div>
    </div>
  </div>

  <!-- ── RIGHT 1/3 ── -->

  <!-- Light Control -->
  <div class="panel area-light col-right">
    <div class="panel-title">// Light Control</div>
    <div class="faders-row">
      <div class="fader-group fader-r">
        <div class="fader-label">RED</div>
        <div class="vslider-wrap"><input type="range" class="vertical" min="0" max="255" value="0" id="fR" oninput="onFader()"></div>
        <input type="number" min="0" max="255" value="0" id="vR" oninput="document.getElementById('fR').value=this.value;onFader();">
      </div>
      <div class="fader-group fader-g">
        <div class="fader-label">GRN</div>
        <div class="vslider-wrap"><input type="range" class="vertical" min="0" max="255" value="0" id="fG" oninput="onFader()"></div>
        <input type="number" min="0" max="255" value="0" id="vG" oninput="document.getElementById('fG').value=this.value;onFader();">
      </div>
      <div class="fader-group fader-b">
        <div class="fader-label">BLU</div>
        <div class="vslider-wrap"><input type="range" class="vertical" min="0" max="255" value="0" id="fB" oninput="onFader()"></div>
        <input type="number" min="0" max="255" value="0" id="vB" oninput="document.getElementById('fB').value=this.value;onFader();">
      </div>
      <div class="fader-group fader-w">
        <div class="fader-label">WHT</div>
        <div class="vslider-wrap"><input type="range" class="vertical" min="0" max="255" value="0" id="fW" oninput="onFader()"></div>
        <input type="number" min="0" max="255" value="0" id="vW" oninput="document.getElementById('fW').value=this.value;onFader();">
      </div>
    </div>
    <div class="preview-row"><div class="led-preview" id="ledPreview"></div></div>
  </div>

  <!-- Motion -->
  <div class="panel area-motion col-right">
    <div class="panel-title">// Motion</div>
    <div class="motion-row">
      <div class="motion-label">PAN</div>
      <input type="range" class="horizontal" min="0" max="180" value="90" id="fPan" oninput="document.getElementById('vPan').value=this.value;onMotion()">
      <input type="number" min="0" max="180" value="90" id="vPan" style="width:52px;color:var(--accent2);" oninput="document.getElementById('fPan').value=this.value;onMotion();">
    </div>
    <div class="motion-row">
      <div class="motion-label">TILT</div>
      <input type="range" class="horizontal" min="0" max="180" value="90" id="fTilt" oninput="document.getElementById('vTilt').value=this.value;onMotion()">
      <input type="number" min="0" max="180" value="90" id="vTilt" style="width:52px;color:var(--accent2);" oninput="document.getElementById('fTilt').value=this.value;onMotion();">
    </div>
  </div>

  <!-- Rainbow — broadcasts to ALL heads -->
  <div class="panel area-rainbow col-right">
    <button class="btn rainbow-btn" id="rainbowBtn" onclick="toggleRainbow()">RAINBOW // ALL HEADS</button>
  </div>

  <!-- Serial -->
  <div class="panel area-serial col-right">
    <div class="panel-title">// Serial</div>
    <div class="serial-row">
      <input type="text" id="cmdInput" placeholder="R:255,G:0,B:0,W:0,PAN:90,TILT:45">
      <button class="btn primary" onclick="sendRaw()">SEND</button>
    </div>
  </div>

  <!-- Art-Net Patch sidebar -->
  <div class="panel area-artpatch col-right" id="artpatch-sidebar-panel">
    <div class="panel-title">// Art-Net Patch</div>
    <div id="ap_list"><div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:8px 0;">No patches</div></div>
  </div>

</div>
<div id="toast"></div>

<!-- ── Cue Edit Modal ── -->
<div id="cueEditModal">
  <div class="modal-box">
    <div class="modal-title">// Edit Targets</div>
    <div id="cueEditList"></div>
    <div style="margin-top:10px;">
      <div style="font-family:var(--mono);font-size:10px;color:var(--text-dim);margin-bottom:5px;">EXTRA IDs (comma-separated):</div>
      <input type="text" id="cueEditFree" placeholder="4, 5, 6" style="width:100%;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:6px 8px;font-family:var(--mono);font-size:12px;border-radius:4px;outline:none;">
    </div>
    <div class="modal-footer">
      <button class="btn primary" onclick="cueEditSave()">SAVE</button>
      <button class="btn" onclick="cueEditClose()">CANCEL</button>
    </div>
  </div>
</div>

<script>
var rainbowActive=false,seqSelectedIds=[],sendTimer=null,_editCueId=null,_cDragId=null;
fetch('/api/status').then(function(r){return r.json();}).then(function(d){if(d.ip)document.getElementById('ipLabel').textContent=d.ip;});
fetch('/api/mode').then(function(r){return r.json();}).then(function(d){var el=document.getElementById('modeLabel');if(el&&d.mode)el.textContent=d.mode.toUpperCase();}).catch(function(){});
function exportCues(){var a=document.createElement('a');a.href='/api/cues/export';a.download='cues.json';a.click();}
function importCues(input){var file=input.files[0];if(!file)return;var r=new FileReader();r.onload=function(e){fetch('/api/cues/import',{method:'POST',headers:{'Content-Type':'application/json'},body:e.target.result}).then(function(r){return r.json();}).then(function(d){toast('Imported: '+d.count+' cues');loadCues();}).catch(function(){toast('Import failed','err');});};r.readAsText(file);input.value='';}
function toast(msg,type){var el=document.getElementById('toast');el.textContent=msg;el.className='show '+(type||'ok');clearTimeout(el._t);el._t=setTimeout(function(){el.className='';},2000);}
function getValues(){return{r:+document.getElementById('fR').value,g:+document.getElementById('fG').value,b:+document.getElementById('fB').value,w:+document.getElementById('fW').value,pan:+document.getElementById('fPan').value,tilt:+document.getElementById('fTilt').value};}
function updatePreview(){var v=getValues(),wr=Math.min(255,v.r+v.w),wg=Math.min(255,v.g+v.w),wb=Math.min(255,v.b+v.w),p=document.getElementById('ledPreview'),hex='rgb('+wr+','+wg+','+wb+')',br=(wr+wg+wb)/3;p.style.background=hex;p.style.boxShadow=br>10?'0 0 '+(20+br/4)+'px '+(8+br/8)+'px '+hex:'none';}
function onFader(){var v=getValues();document.getElementById('vR').value=v.r;document.getElementById('vG').value=v.g;document.getElementById('vB').value=v.b;document.getElementById('vW').value=v.w;updatePreview();debounceSend();}
function onMotion(){var v=getValues();document.getElementById('vPan').value=v.pan;document.getElementById('vTilt').value=v.tilt;debounceSend();}
function debounceSend(){clearTimeout(sendTimer);sendTimer=setTimeout(sendCurrent,40);}
function sendCurrent(){var v=getValues();var cmd='R:'+v.r+',G:'+v.g+',B:'+v.b+',W:'+v.w+',PAN:'+v.pan+',TILT:'+v.tilt;var macs=(typeof nh_getSelectedMACs==='function')?nh_getSelectedMACs():[];if(macs.length){fetch('/api/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:cmd,targets:macs})});}else{sendCommand(cmd);}}
function sendCommand(cmd){fetch('/api/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:cmd})});}
function toggleRainbow(){
  rainbowActive=!rainbowActive;
  document.getElementById('rainbowBtn').classList.toggle('active',rainbowActive);
  // POST to /api/rainbow — leader applies locally + broadcasts to all followers
  fetch('/api/rainbow',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:rainbowActive})});
  toast(rainbowActive?'Rainbow ON \u2014 all heads':'Rainbow OFF');
}
function loadCues(){fetch('/api/cues').then(function(r){return r.json();}).then(renderCues);}
function escHtml(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
function renderCues(cues){var list=document.getElementById('cueList');if(!cues.length){list.innerHTML='<div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:20px 0;">No cues saved yet</div>';return;}list.innerHTML='';cues.forEach(function(cue){var inSeq=seqSelectedIds.indexOf(cue.id)>=0,wr=Math.min(255,cue.r+cue.w),wg=Math.min(255,cue.g+cue.w),wb=Math.min(255,cue.b+cue.w),el=document.createElement('div');el.className='cue-item'+(inSeq?' seq-selected':'');el.draggable=true;el.dataset.cueId=cue.id;var ftStr=(cue.fixTargets&&cue.fixTargets.length)?' <span style="color:var(--accent3)">\u2192 '+(cue.fixTargets.map(function(id){return id===0?'ALL':'#'+id;}).join(' '))+'</span>':'';el.innerHTML='<div class="cue-drag" title="Drag to reorder">&#8942;</div><div class="cue-swatch" style="background:rgb('+wr+','+wg+','+wb+')"></div><div class="cue-info"><div class="cue-name">'+escHtml(cue.name)+'</div><div class="cue-meta">P:'+cue.pan+'deg T:'+cue.tilt+'deg W:'+cue.w+ftStr+'</div></div><div class="cue-actions"><button class="icon-btn" title="Edit targets" onclick="editCue('+cue.id+','+JSON.stringify(cue.fixTargets||[0])+')">&#9998;</button><button class="icon-btn" onclick="toggleSeqCue('+cue.id+')">+</button><button class="icon-btn" onclick="fireCue('+cue.id+')">GO</button><button class="icon-btn del" onclick="deleteCue('+cue.id+')">X</button></div>';el.addEventListener('dragstart',function(e){_cDragId=cue.id;e.dataTransfer.effectAllowed='move';el.classList.add('dragging');});el.addEventListener('dragend',function(){el.classList.remove('dragging');document.querySelectorAll('.cue-item.drag-over').forEach(function(x){x.classList.remove('drag-over');});});el.addEventListener('dragover',function(e){e.preventDefault();e.dataTransfer.dropEffect='move';el.classList.add('drag-over');});el.addEventListener('dragleave',function(e){if(!el.contains(e.relatedTarget))el.classList.remove('drag-over');});el.addEventListener('drop',function(e){e.preventDefault();el.classList.remove('drag-over');if(_cDragId===cue.id)return;var items=document.querySelectorAll('.cue-item[data-cue-id]'),order=[];items.forEach(function(x){order.push(+x.dataset.cueId);});var fi=order.indexOf(_cDragId),ti=order.indexOf(cue.id);if(fi<0||ti<0)return;order.splice(fi,1);order.splice(ti,0,_cDragId);fetch('/api/cues/reorder',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({order:order})}).then(function(){loadCues();});});list.appendChild(el);});}
function saveCue(){var name=document.getElementById('cueName').value.trim();if(!name){toast('Enter a cue name','err');return;}var v=getValues();var ft=(typeof nh_getSelectedFixIDs==='function')?nh_getSelectedFixIDs():[];if(!ft.length)ft=[0];fetch('/api/cues',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:name,r:v.r,g:v.g,b:v.b,w:v.w,pan:v.pan,tilt:v.tilt,fixTargets:ft})}).then(function(){document.getElementById('cueName').value='';toast('Cue saved');loadCues();});}
function fireCue(id){fetch('/api/cues/'+id+'/fire',{method:'POST'}).then(function(r){return r.json();}).then(function(d){if(d.status==='ok'){toast('Cue fired');fetch('/api/cues').then(function(r){return r.json();}).then(function(cues){var cue=cues.find(function(c){return c.id===id;});if(cue){document.getElementById('fR').value=cue.r;document.getElementById('fG').value=cue.g;document.getElementById('fB').value=cue.b;document.getElementById('fW').value=cue.w;document.getElementById('fPan').value=cue.pan;document.getElementById('fTilt').value=cue.tilt;onFader();onMotion();}});}});}
function deleteCue(id){seqSelectedIds=seqSelectedIds.filter(function(i){return i!==id;});fetch('/api/cues/'+id,{method:'DELETE'}).then(function(){toast('Cue deleted');loadCues();});}
function toggleSeqCue(id){var idx=seqSelectedIds.indexOf(id);if(idx>=0)seqSelectedIds.splice(idx,1);else seqSelectedIds.push(id);loadCues();document.getElementById('seqStatus').textContent=seqSelectedIds.length+' cue(s) in sequence';}
function startSequencer(){if(!seqSelectedIds.length){toast('Select cues first','err');return;}var interval=+document.getElementById('seqInterval').value,loop=document.getElementById('seqLoop').checked;fetch('/api/sequencer/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cue_ids:seqSelectedIds,interval_ms:interval,loop:loop})}).then(function(){document.getElementById('seqStatus').textContent='Running - '+seqSelectedIds.length+' cues';document.getElementById('seqStartBtn').classList.add('active');toast('Sequencer started');});}
function stopSequencer(){fetch('/api/sequencer/stop',{method:'POST'}).then(function(){document.getElementById('seqStatus').textContent='Sequencer idle';document.getElementById('seqStartBtn').classList.remove('active');toast('Sequencer stopped');});}
function editCue(id,curTargets){_editCueId=id;fetch('/api/fixtures').then(function(r){return r.json();}).catch(function(){return[];}).then(function(fixtures){var list=document.getElementById('cueEditList');var html='<label class="modal-check-row"><input type="checkbox" id="cue_all" '+(curTargets.indexOf(0)>=0?'checked':'')+'><span style="color:var(--accent)">ALL (broadcast to everyone)</span></label>';fixtures.forEach(function(f){if(f.id<=0)return;var chk=curTargets.indexOf(f.id)>=0;html+='<label class="modal-check-row"><input type="checkbox" class="cue-fix-cb" value="'+f.id+'" '+(chk?'checked':'')+'>'+f.id+' &#8212; '+escHtml(f.name||'?')+(f.online?' <span style="color:var(--success);font-size:9px">&#9679;</span>':' <span style="color:var(--text-dim);font-size:9px">&#9675;</span>')+'</label>';});list.innerHTML=html;var poolIds=fixtures.map(function(f){return f.id;});var free=curTargets.filter(function(id){return id>0&&poolIds.indexOf(id)<0;});document.getElementById('cueEditFree').value=free.join(', ');document.getElementById('cueEditModal').classList.add('open');});}
function cueEditClose(){document.getElementById('cueEditModal').classList.remove('open');_editCueId=null;}
function cueEditSave(){if(!_editCueId)return;var ft=[];if(document.getElementById('cue_all').checked){ft=[0];}else{document.querySelectorAll('.cue-fix-cb:checked').forEach(function(cb){ft.push(parseInt(cb.value));});var freeVal=document.getElementById('cueEditFree').value;freeVal.split(',').forEach(function(s){var n=parseInt(s.trim());if(n>0&&ft.indexOf(n)<0)ft.push(n);});}if(!ft.length)ft=[0];fetch('/api/cues/'+_editCueId+'/targets',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({fixTargets:ft})}).then(function(){cueEditClose();toast('Targets updated');loadCues();});}
function sendRaw(){var cmd=document.getElementById('cmdInput').value.trim();if(!cmd)return;sendCommand(cmd);toast('Sent: '+cmd);}
document.getElementById('cmdInput').addEventListener('keydown',function(e){if(e.key==='Enter')sendRaw();});
updatePreview();loadCues();
// Load network heads plugin
function loadModule(url, id) {
  fetch(url).then(function(r){return r.text();}).then(function(html){
    var el=document.getElementById(id);if(!el)return;
    el.innerHTML=html;
    el.querySelectorAll('script').forEach(function(old){
      var s=document.createElement('script');s.textContent=old.textContent;
      old.parentNode.replaceChild(s,old);
    });
  }).catch(function(){
    var el=document.getElementById(id);
    if(el)el.innerHTML='<div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:20px 0;">// Network heads plugin not available</div>';
  });
}
loadModule('/plugins/wifi/discovery_panel.html','module-container');
loadModule('/plugins/artnet/panel.html','artnet-module-container');
// ── Art-Net patch sidebar ─────────────────────────────────────────
var _FIX_HUES=[200,30,120,270,0,60,160,300,45,330];
function loadArtpatch(){
  Promise.all([
    fetch('/api/artnet/patch').then(function(r){return r.json();}),
    fetch('/api/fixtures').then(function(r){return r.json();}).catch(function(){return[];})
  ]).then(function(res){
    var patches=res[0],fixtures=res[1];
    var el=document.getElementById('ap_list');if(!el)return;
    var active=document.activeElement;if(active&&el.contains(active))return;
    if(!patches.length){el.innerHTML='<div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:8px 0;">No patches</div>';return;}
    var fmap={};fixtures.forEach(function(f){fmap[f.id]=f.name||('Fix#'+f.id);});
    var html='<table class="ap-table"><thead><tr><th></th><th>Fix#</th><th>Name</th><th>Uni</th><th>Addr</th><th></th></tr></thead><tbody>';
    patches.forEach(function(p){
      var hue=_FIX_HUES[(p.fixID-1)%_FIX_HUES.length];
      var col='hsl('+hue+',68%,52%)';
      html+='<tr>'
        +'<td><div style="width:8px;height:8px;border-radius:2px;background:'+col+'"></div></td>'
        +'<td style="color:var(--accent2);font-weight:700;">'+p.fixID+'</td>'
        +'<td style="max-width:70px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">'+(fmap[p.fixID]||('Fix#'+p.fixID))+'</td>'
        +'<td><input type="number" class="ap-num" value="'+p.universe+'" min="0" max="32767" onchange="ap_update('+p.fixID+',+this.value,null)"></td>'
        +'<td><input type="number" class="ap-num" style="color:var(--accent);" value="'+p.startAddr+'" min="1" max="512" onchange="ap_update('+p.fixID+',null,+this.value)"></td>'
        +'<td><button class="ap-del" onclick="ap_del('+p.fixID+')">&#10005;</button></td>'
        +'</tr>';
    });
    html+='</tbody></table>';
    el.innerHTML=html;
  }).catch(function(){});
}
window.ap_update=function(fixID,uni,addr){
  fetch('/api/artnet/patch').then(function(r){return r.json();}).then(function(patches){
    var p=patches.find(function(x){return x.fixID===fixID;});if(!p)return;
    var data={universe:uni!==null?uni:p.universe,startAddr:addr!==null?addr:p.startAddr};
    fetch('/api/artnet/patch/'+fixID,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})
      .then(function(){loadArtpatch();if(typeof toast==='function')toast('Patch updated');});
  });
};
window.ap_del=function(fixID){
  fetch('/api/artnet/patch/'+fixID,{method:'DELETE'}).then(function(){
    loadArtpatch();if(typeof toast==='function')toast('Patch removed');
  });
};
loadArtpatch();setInterval(loadArtpatch,3000);
var _artnetWasActive=false;
function an_pollStatus(){fetch('/api/artnet/status').then(function(r){return r.json();}).then(function(d){if(d.active===_artnetWasActive)return;_artnetWasActive=d.active;document.getElementById('artnet-bar').classList.toggle('visible',d.active);document.getElementById('artnet-bar-count').textContent=d.active?d.patchCount+' fixture(s)':'';['.area-light','.area-motion','.area-rainbow','.area-serial','.area-sequencer'].forEach(function(sel){var el=document.querySelector(sel);if(el)el.classList.toggle('artnet-locked',d.active);});}).catch(function(){});}
setInterval(an_pollStatus,500);
</script>
</body>
</html>
)=====";
