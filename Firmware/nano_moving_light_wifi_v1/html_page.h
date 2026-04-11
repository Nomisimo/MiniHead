#pragma once

// ── Embedded HTML UI ─────────────────────────────────────────────
// Stored in flash (PROGMEM). Served by the ESP32 web server at GET /
// Keep this file in the same folder as the .ino file.

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
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
  header{display:flex;align-items:center;justify-content:space-between;padding:16px 24px;border-bottom:1px solid var(--border);background:var(--surface);position:sticky;top:0;z-index:100;}
  .logo{font-family:var(--mono);font-size:18px;color:var(--accent);letter-spacing:2px;text-transform:uppercase;}
  .logo span{color:var(--text-dim);}
  .connection-bar{display:flex;align-items:center;gap:10px;font-family:var(--mono);font-size:12px;}
  .status-dot{width:8px;height:8px;border-radius:50%;background:var(--success);box-shadow:0 0 8px var(--success);animation:pulse 2s infinite;}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
  .ip-label{color:var(--accent);letter-spacing:1px;}
  .btn{padding:7px 16px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:12px;cursor:pointer;border-radius:4px;letter-spacing:1px;transition:all 0.15s;text-transform:uppercase;}
  .btn:hover{border-color:var(--accent);color:var(--accent);}
  .btn.primary{border-color:var(--accent);color:var(--accent);background:rgba(0,229,255,0.08);}
  .btn.primary:hover{background:rgba(0,229,255,0.18);}
  .btn.danger{border-color:var(--danger);color:var(--danger);}
  .btn.danger:hover{background:rgba(239,68,68,0.1);}
  .btn.success{border-color:var(--success);color:var(--success);}
  .btn.success:hover{background:rgba(34,197,94,0.1);}
  .btn.active{background:rgba(168,85,247,0.2);border-color:var(--accent3);color:var(--accent3);}
  .main{display:grid;grid-template-columns:1fr 320px;grid-template-rows:auto 1fr;gap:1px;background:var(--border);min-height:calc(100vh - 57px);}
  .panel{background:var(--bg);padding:20px;}
  .panel-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:20px;padding-bottom:10px;border-bottom:1px solid var(--border);}
  .faders-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin-bottom:24px;}
  .fader-group{display:flex;flex-direction:column;align-items:center;gap:10px;}
  .fader-label{font-family:var(--mono);font-size:11px;color:var(--text-dim);letter-spacing:2px;}
  .fader-value{font-family:var(--mono);font-size:20px;font-weight:600;color:var(--text);min-width:40px;text-align:center;}
  .vslider-wrap{position:relative;width:40px;height:160px;display:flex;align-items:center;justify-content:center;}
  input[type=range].vertical{writing-mode:vertical-lr;direction:rtl;width:40px;height:160px;cursor:pointer;-webkit-appearance:slider-vertical;appearance:slider-vertical;}
  input[type=range]{-webkit-appearance:none;appearance:none;background:transparent;cursor:pointer;}
  .hslider-wrap{width:100%;}
  input[type=range].horizontal{-webkit-appearance:none;appearance:none;width:100%;height:6px;background:var(--surface2);border-radius:3px;outline:none;cursor:pointer;}
  input[type=range].horizontal::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:var(--accent2);box-shadow:0 0 10px var(--accent2);border:2px solid var(--bg);transition:transform 0.1s;}
  input[type=range].horizontal::-webkit-slider-thumb:hover{transform:scale(1.2);}
  input[type=range].vertical::-webkit-slider-thumb{-webkit-appearance:none;width:28px;height:14px;border-radius:3px;background:var(--accent);box-shadow:0 0 12px var(--accent);border:2px solid var(--bg);transition:transform 0.1s;}
  input[type=range].vertical::-webkit-slider-thumb:hover{transform:scaleX(1.2);}
  .fader-r input::-webkit-slider-thumb{background:#ff4444;box-shadow:0 0 12px #ff4444;}
  .fader-g input::-webkit-slider-thumb{background:#44ff44;box-shadow:0 0 12px #44ff44;}
  .fader-b input::-webkit-slider-thumb{background:#4488ff;box-shadow:0 0 12px #4488ff;}
  .fader-w input::-webkit-slider-thumb{background:#ffffff;box-shadow:0 0 12px #ffffff;}
  .led-preview{width:80px;height:80px;border-radius:50%;margin:0 auto 20px;border:2px solid var(--border);transition:background 0.1s,box-shadow 0.1s;background:#000;}
  .motion-section{margin-top:24px;border-top:1px solid var(--border);padding-top:20px;}
  .motion-row{display:grid;grid-template-columns:40px 1fr 48px;align-items:center;gap:12px;margin-bottom:16px;}
  .motion-label{font-family:var(--mono);font-size:11px;color:var(--text-dim);letter-spacing:2px;}
  .motion-val{font-family:var(--mono);font-size:14px;color:var(--accent2);text-align:right;}
  .rainbow-btn{width:100%;margin-top:20px;padding:12px;font-size:14px;letter-spacing:3px;background:linear-gradient(90deg,#ff000033,#ffff0033,#00ff0033,#00ffff33,#0000ff33,#ff00ff33);border:1px solid var(--border);color:var(--text);transition:all 0.3s;}
  .rainbow-btn.active{background:linear-gradient(90deg,#ff0000aa,#ffff00aa,#00ff00aa,#00ffffaa,#0000ffaa,#ff00ffaa);animation:rainbow-shift 2s linear infinite;color:#fff;border-color:transparent;}
  @keyframes rainbow-shift{0%{filter:hue-rotate(0deg)}100%{filter:hue-rotate(360deg)}}
  .cue-panel{grid-row:1/3;display:flex;flex-direction:column;gap:1px;}
  .cue-list{flex:1;overflow-y:auto;display:flex;flex-direction:column;gap:6px;max-height:380px;padding-right:4px;}
  .cue-list::-webkit-scrollbar{width:4px;}
  .cue-list::-webkit-scrollbar-track{background:var(--surface);}
  .cue-list::-webkit-scrollbar-thumb{background:var(--border);border-radius:2px;}
  .cue-item{display:flex;align-items:center;gap:8px;padding:10px 12px;background:var(--surface2);border:1px solid var(--border);border-radius:4px;transition:border-color 0.15s;cursor:default;}
  .cue-item:hover{border-color:var(--accent3);}
  .cue-item.seq-selected{border-color:var(--accent3);background:rgba(168,85,247,0.1);}
  .cue-swatch{width:28px;height:28px;border-radius:4px;flex-shrink:0;border:1px solid rgba(255,255,255,0.1);}
  .cue-info{flex:1;min-width:0;}
  .cue-name{font-size:13px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .cue-meta{font-family:var(--mono);font-size:10px;color:var(--text-dim);margin-top:2px;}
  .cue-actions{display:flex;gap:4px;}
  .icon-btn{width:26px;height:26px;border:1px solid var(--border);background:transparent;color:var(--text-dim);border-radius:3px;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;transition:all 0.15s;}
  .icon-btn:hover{border-color:var(--accent);color:var(--accent);}
  .icon-btn.del:hover{border-color:var(--danger);color:var(--danger);}
  .save-cue-form{display:flex;gap:8px;margin-top:14px;}
  input[type=text]{flex:1;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:7px 10px;font-family:var(--mono);font-size:12px;border-radius:4px;outline:none;transition:border-color 0.15s;}
  input[type=text]:focus{border-color:var(--accent);}
  .seq-section{margin-top:20px;border-top:1px solid var(--border);padding-top:16px;}
  .seq-controls{display:flex;flex-direction:column;gap:10px;}
  .seq-row{display:flex;align-items:center;gap:8px;}
  .seq-label{font-family:var(--mono);font-size:10px;color:var(--text-dim);width:60px;flex-shrink:0;}
  input[type=number]{width:80px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:6px 8px;font-family:var(--mono);font-size:12px;border-radius:4px;outline:none;}
  .seq-hint{font-size:11px;color:var(--text-dim);margin-top:4px;}
  .seq-buttons{display:flex;gap:8px;margin-top:8px;}
  #toast{position:fixed;bottom:24px;right:24px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:10px 18px;font-family:var(--mono);font-size:12px;border-radius:4px;opacity:0;transition:opacity 0.2s;z-index:999;pointer-events:none;}
  #toast.show{opacity:1;}
  #toast.ok{border-color:var(--success);color:var(--success);}
  #toast.err{border-color:var(--danger);color:var(--danger);}
</style>
</head>
<body>
<header>
  <div class="logo">NANO<span>/</span>ML <span style="font-size:11px;color:var(--text-dim)">CONTROLLER</span></div>
  <div class="connection-bar">
    <div class="status-dot"></div>
    <span class="ip-label" id="ipLabel">...</span>
    <span style="color:var(--text-dim)">ESP32 WiFi</span>
  </div>
</header>
<div class="main">
  <div class="panel">
    <div class="panel-title">// Light Control</div>
    <div style="display:flex;gap:24px;align-items:flex-start;">
      <div style="flex:1;">
        <div class="faders-grid">
          <div class="fader-group fader-r">
            <div class="fader-label">RED</div>
            <div class="vslider-wrap"><input type="range" class="vertical" min="0" max="255" value="0" id="fR" oninput="onFader()"></div>
            <input type="number" min="0" max="255" value="0" id="vR" style="width:48px;text-align:center;font-family:var(--mono);font-size:16px;background:var(--surface2);border:1px solid var(--border);color:var(--text);border-radius:4px;padding:2px;" oninput="document.getElementById('fR').value=this.value;onFader();">
          </div>
          <div class="fader-group fader-g">
            <div class="fader-label">GREEN</div>
            <div class="vslider-wrap"><input type="range" class="vertical" min="0" max="255" value="0" id="fG" oninput="onFader()"></div>
            <input type="number" min="0" max="255" value="0" id="vG" style="width:48px;text-align:center;font-family:var(--mono);font-size:16px;background:var(--surface2);border:1px solid var(--border);color:var(--text);border-radius:4px;padding:2px;" oninput="document.getElementById('fG').value=this.value;onFader();">
          </div>
          <div class="fader-group fader-b">
            <div class="fader-label">BLUE</div>
            <div class="vslider-wrap"><input type="range" class="vertical" min="0" max="255" value="0" id="fB" oninput="onFader()"></div>
            <input type="number" min="0" max="255" value="0" id="vB" style="width:48px;text-align:center;font-family:var(--mono);font-size:16px;background:var(--surface2);border:1px solid var(--border);color:var(--text);border-radius:4px;padding:2px;" oninput="document.getElementById('fB').value=this.value;onFader();">
          </div>
          <div class="fader-group fader-w" style="grid-column:2;">
            <div class="fader-label">WHITE</div>
            <div class="vslider-wrap"><input type="range" class="vertical" min="0" max="255" value="0" id="fW" oninput="onFader()"></div>
            <input type="number" min="0" max="255" value="0" id="vW" style="width:48px;text-align:center;font-family:var(--mono);font-size:16px;background:var(--surface2);border:1px solid var(--border);color:var(--text);border-radius:4px;padding:2px;" oninput="document.getElementById('fW').value=this.value;onFader();">
          </div>
        </div>
      </div>
      <div style="display:flex;flex-direction:column;align-items:center;gap:12px;padding-top:10px;">
        <div class="led-preview" id="ledPreview"></div>
        <div style="font-family:var(--mono);font-size:10px;color:var(--text-dim)">PREVIEW</div>
      </div>
    </div>
<div class="motion-section">
  <div class="panel-title" style="margin-bottom:16px;">// Motion</div>
  <div class="motion-row">
    <div class="motion-label">PAN</div>
    <div class="hslider-wrap"><input type="range" class="horizontal" min="0" max="180" value="90" id="fPan" oninput="document.getElementById('vPan').value=this.value;onMotion()"></div>
    <input type="number" min="0" max="180" value="90" id="vPan" style="width:52px;text-align:center;font-family:var(--mono);font-size:13px;background:var(--surface2);border:1px solid var(--border);color:var(--accent2);border-radius:4px;padding:2px;" oninput="document.getElementById('fPan').value=this.value;onMotion();">
  </div>
  <div class="motion-row">
    <div class="motion-label">TILT</div>
    <div class="hslider-wrap"><input type="range" class="horizontal" min="0" max="180" value="90" id="fTilt" oninput="document.getElementById('vTilt').value=this.value;onMotion()"></div>
    <input type="number" min="0" max="180" value="90" id="vTilt" style="width:52px;text-align:center;font-family:var(--mono);font-size:13px;background:var(--surface2);border:1px solid var(--border);color:var(--accent2);border-radius:4px;padding:2px;" oninput="document.getElementById('fTilt').value=this.value;onMotion();">
  </div>
</div>
    <button class="btn rainbow-btn" id="rainbowBtn" onclick="toggleRainbow()">RAINBOW TEST</button>
  </div>
  <div class="panel cue-panel">
    <div class="panel-title">// Cues</div>
    <div class="cue-list" id="cueList"></div>
    <div class="save-cue-form">
      <input type="text" id="cueName" placeholder="Cue name..." maxlength="30">
      <button class="btn primary" onclick="saveCue()">SAVE</button>
    </div>
    <div class="seq-section">
      <div class="panel-title" style="margin-bottom:12px;">// Sequencer</div>
      <div class="seq-hint" style="margin-bottom:10px;">Click the diamond on cues to add to sequence</div>
      <div class="seq-controls">
        <div class="seq-row">
          <div class="seq-label">INTERVAL</div>
          <input type="number" id="seqInterval" value="1000" min="100" max="30000" step="100">
          <div style="font-family:var(--mono);font-size:10px;color:var(--text-dim)">ms</div>
        </div>
        <div class="seq-row">
          <div class="seq-label">LOOP</div>
          <label style="display:flex;align-items:center;gap:6px;cursor:pointer;">
            <input type="checkbox" id="seqLoop" checked style="accent-color:var(--accent3);">
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
  </div>
</div>
<div id="toast"></div>
<script>
var rainbowActive=false,seqSelectedIds=[],sendTimer=null;
fetch('/api/status').then(function(r){return r.json();}).then(function(d){if(d.ip)document.getElementById('ipLabel').textContent=d.ip;});
function toast(msg,type){var el=document.getElementById('toast');el.textContent=msg;el.className='show '+(type||'ok');clearTimeout(el._t);el._t=setTimeout(function(){el.className='';},2000);}
function getValues(){return{r:+document.getElementById('fR').value,g:+document.getElementById('fG').value,b:+document.getElementById('fB').value,w:+document.getElementById('fW').value,pan:+document.getElementById('fPan').value,tilt:+document.getElementById('fTilt').value};}
function updatePreview(){var v=getValues(),wr=Math.min(255,v.r+v.w),wg=Math.min(255,v.g+v.w),wb=Math.min(255,v.b+v.w),p=document.getElementById('ledPreview'),hex='rgb('+wr+','+wg+','+wb+')',br=(wr+wg+wb)/3;p.style.background=hex;p.style.boxShadow=br>10?'0 0 '+(20+br/4)+'px '+(8+br/8)+'px '+hex:'none';}
function onFader(){var v=getValues();document.getElementById('vR').value=v.r;document.getElementById('vG').value=v.g;document.getElementById('vB').value=v.b;document.getElementById('vW').value=v.w;updatePreview();debounceSend();}
function onMotion(){var v=getValues();document.getElementById('vPan').value=v.pan;document.getElementById('vTilt').value=v.tilt;debounceSend();}
function debounceSend(){clearTimeout(sendTimer);sendTimer=setTimeout(sendCurrent,40);}
function sendCurrent(){var v=getValues();sendCommand('R:'+v.r+',G:'+v.g+',B:'+v.b+',W:'+v.w+',PAN:'+v.pan+',TILT:'+v.tilt);}
function sendCommand(cmd){fetch('/api/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:cmd})});}
function toggleRainbow(){rainbowActive=!rainbowActive;document.getElementById('rainbowBtn').classList.toggle('active',rainbowActive);sendCommand('RAINBOW:'+(rainbowActive?1:0));toast(rainbowActive?'Rainbow on':'Rainbow off');}
function loadCues(){fetch('/api/cues').then(function(r){return r.json();}).then(renderCues);}
function escHtml(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
function renderCues(cues){var list=document.getElementById('cueList');if(!cues.length){list.innerHTML='<div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:20px 0;">No cues saved yet</div>';return;}list.innerHTML='';cues.forEach(function(cue){var inSeq=seqSelectedIds.indexOf(cue.id)>=0,wr=Math.min(255,cue.r+cue.w),wg=Math.min(255,cue.g+cue.w),wb=Math.min(255,cue.b+cue.w),el=document.createElement('div');el.className='cue-item'+(inSeq?' seq-selected':'');el.innerHTML='<div class="cue-swatch" style="background:rgb('+wr+','+wg+','+wb+')"></div><div class="cue-info"><div class="cue-name">'+escHtml(cue.name)+'</div><div class="cue-meta">P:'+cue.pan+'deg T:'+cue.tilt+'deg W:'+cue.w+'</div></div><div class="cue-actions"><button class="icon-btn" onclick="toggleSeqCue('+cue.id+')">+</button><button class="icon-btn" onclick="fireCue('+cue.id+')">GO</button><button class="icon-btn del" onclick="deleteCue('+cue.id+')">X</button></div>';list.appendChild(el);});}
function saveCue(){var name=document.getElementById('cueName').value.trim();if(!name){toast('Enter a cue name','err');return;}var v=getValues();fetch('/api/cues',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:name,r:v.r,g:v.g,b:v.b,w:v.w,pan:v.pan,tilt:v.tilt})}).then(function(){document.getElementById('cueName').value='';toast('Cue saved');loadCues();});}
function fireCue(id){fetch('/api/cues/'+id+'/fire',{method:'POST'}).then(function(r){return r.json();}).then(function(d){if(d.status==='ok'){toast('Cue fired');fetch('/api/cues').then(function(r){return r.json();}).then(function(cues){var cue=cues.find(function(c){return c.id===id;});if(cue){document.getElementById('fR').value=cue.r;document.getElementById('fG').value=cue.g;document.getElementById('fB').value=cue.b;document.getElementById('fW').value=cue.w;document.getElementById('fPan').value=cue.pan;document.getElementById('fTilt').value=cue.tilt;onFader();onMotion();}});}});}
function deleteCue(id){seqSelectedIds=seqSelectedIds.filter(function(i){return i!==id;});fetch('/api/cues/'+id,{method:'DELETE'}).then(function(){toast('Cue deleted');loadCues();});}
function toggleSeqCue(id){var idx=seqSelectedIds.indexOf(id);if(idx>=0)seqSelectedIds.splice(idx,1);else seqSelectedIds.push(id);loadCues();document.getElementById('seqStatus').textContent=seqSelectedIds.length+' cue(s) in sequence';}
function startSequencer(){if(!seqSelectedIds.length){toast('Select cues first','err');return;}var interval=+document.getElementById('seqInterval').value,loop=document.getElementById('seqLoop').checked;fetch('/api/sequencer/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cue_ids:seqSelectedIds,interval_ms:interval,loop:loop})}).then(function(){document.getElementById('seqStatus').textContent='Running - '+seqSelectedIds.length+' cues';document.getElementById('seqStartBtn').classList.add('active');toast('Sequencer started');});}
function stopSequencer(){fetch('/api/sequencer/stop',{method:'POST'}).then(function(){document.getElementById('seqStatus').textContent='Sequencer idle';document.getElementById('seqStartBtn').classList.remove('active');toast('Sequencer stopped');});}
function sendRaw() {
  var cmd = document.getElementById('cmdInput').value.trim();
  if (!cmd) return;
  sendCommand(cmd);
  toast('Sent: ' + cmd);
}
document.getElementById('cmdInput').addEventListener('keydown', function(e) {
  if (e.key === 'Enter') sendRaw();
});
updatePreview();loadCues();
</script>
<div style="position:fixed;bottom:24px;left:24px;display:flex;gap:8px;z-index:998;">
  <input type="text" id="cmdInput" placeholder="R:255,G:0,B:0,W:0,PAN:90,TILT:45" style="width:320px;">
  <button class="btn primary" onclick="sendRaw()">SEND</button>
</div>
</body>
</html>
)=====";