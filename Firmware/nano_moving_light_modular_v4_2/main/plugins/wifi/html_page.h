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
<link rel="stylesheet" href="/theme">
<style>
  *{margin:0;padding:0;box-sizing:border-box;}
  body{min-height:100vh;overflow-x:hidden;}
  header{display:flex;align-items:center;justify-content:space-between;position:sticky;top:0;z-index:100;}
  .connection-bar{display:flex;align-items:center;}
  .status-dot{flex-shrink:0;}
  .main{display:grid;grid-template-columns:1fr;grid-template-areas:"light" "motion" "rainbow" "serial" "cues" "sequencer" "future";}
  @media(min-width:900px){
    .main{grid-template-columns:2fr 1fr;grid-template-rows:auto auto auto auto 1fr;grid-template-areas:"cues light" "sequencer motion" "future rainbow" "future serial" "future .";min-height:calc(100vh - 49px);}
  }
  .area-light{grid-area:light;}.area-motion{grid-area:motion;}.area-rainbow{grid-area:rainbow;}
  .area-serial{grid-area:serial;}.area-cues{grid-area:cues;}.area-sequencer{grid-area:sequencer;}.area-future{grid-area:future;}
  .area-light,.area-motion,.area-rainbow,.area-serial,.area-sequencer{position:relative;}
  .faders-row{display:grid;grid-template-columns:repeat(4,1fr);align-items:end;}
  .fader-group{display:flex;flex-direction:column;align-items:center;}
  .vslider-wrap{width:36px;height:140px;display:flex;align-items:center;justify-content:center;}
  input[type=range].vertical{writing-mode:vertical-lr;direction:rtl;width:36px;height:140px;-webkit-appearance:slider-vertical;appearance:slider-vertical;cursor:pointer;}
  input[type=range]{cursor:pointer;}
  .preview-row{display:flex;justify-content:center;}
  .motion-row{display:grid;grid-template-columns:48px 1fr 60px;align-items:center;}
  .cue-list{display:flex;flex-direction:column;max-height:300px;overflow-y:auto;}
  .cue-item{display:flex;align-items:center;}
  .cue-info{flex:1;min-width:0;}
  .cue-name{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .cue-actions{display:flex;}
  .save-cue-form{display:flex;}
  .seq-controls{display:flex;flex-direction:column;}
  .seq-row{display:flex;align-items:center;}
  .seq-label{width:70px;flex-shrink:0;}
  .seq-buttons{display:flex;}
  .seq-buttons .btn{flex:1;}
  .serial-row{display:flex;}
  #cueEditModal{position:fixed;inset:0;z-index:600;display:none;align-items:center;justify-content:center;}
  #cueEditModal.open{display:flex;}
  .modal-box{min-width:260px;max-width:380px;width:90%;max-height:80vh;overflow-y:auto;}
  .modal-check-row{display:flex;align-items:center;cursor:pointer;}
  .modal-footer{display:flex;}
  #toast{position:fixed;bottom:16px;right:16px;left:16px;opacity:0;z-index:999;pointer-events:none;text-align:center;}
  #toast.show{opacity:1;}
  #artnet-bar{display:none;align-items:center;}
  #artnet-bar.visible{display:flex;}
  .artnet-locked{pointer-events:none!important;opacity:0.38;user-select:none;}
</style>
</head>
<body>
<header>
  <div class="logo">NANO<span>/</span>ML <span style="font-size:10px;color:var(--text-dim)">CTRL</span></div>
  <div class="connection-bar">
    <div class="status-dot"></div>
    <span class="ip-label" id="ipLabel">...</span>
    <span style="color:var(--text-dim)">ESP32</span>
  </div>
</header>
<div id="artnet-bar">
  <div id="artnet-bar-dot"></div>
  <span>ART-NET</span>
  <span id="artnet-live"></span>
  <span id="artnet-bar-count"></span>
</div>

<div class="main">

  <!-- ── LEFT 2/3 ── -->

  <!-- Cues -->
  <div class="panel area-cues">
    <div class="panel-title">// Cues</div><hr>
    <div class="cue-list" id="cueList"></div>
    <div class="save-cue-form">
      <input type="text" id="cueName" placeholder="Cue name..." maxlength="30">
      <button class="btn primary" onclick="saveCue()">SAVE</button>
    </div>
  </div>

  <!-- Sequencer -->
  <div class="panel area-sequencer">
    <div class="panel-title">// Sequencer</div><hr>
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

  <!-- Modules: Network Heads + Art-Net Patch + Log Config -->
  <div class="area-future" style="background:var(--bg)">
    <div class="panel" id="module-container" style="border-bottom:1px solid var(--border)">
      <div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:20px 0;">Loading...</div>
    </div>
    <div class="panel" id="artnet-module-container" style="border-bottom:1px solid var(--border)">
      <div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:20px 0;">Loading Art-Net...</div>
    </div>
    <div class="panel" id="log-module-container">
      <div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:20px 0;">Loading Log Config...</div>
    </div>
  </div>

  <!-- ── RIGHT 1/3 ── -->

  <!-- Light Control -->
  <div class="panel area-light col-right">
    <div class="panel-title">// Light Control</div><hr>
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
    <div class="panel-title">// Motion</div><hr>
    <div class="motion-row">
      <div class="motion-label">PAN</div>
      <input type="range" class="horizontal" min="0" max="270" value="135" id="fPan" oninput="document.getElementById('vPan').value=this.value;onMotion()">
      <input type="number" min="0" max="270" value="135" id="vPan" style="width:52px;color:var(--accent2);" oninput="document.getElementById('fPan').value=this.value;onMotion();">
    </div>
    <div class="motion-row">
      <div class="motion-label">TILT</div>
      <input type="range" class="horizontal" min="0" max="270" value="135" id="fTilt" oninput="document.getElementById('vTilt').value=this.value;onMotion()">
      <input type="number" min="0" max="270" value="135" id="vTilt" style="width:52px;color:var(--accent2);" oninput="document.getElementById('fTilt').value=this.value;onMotion();">
    </div>
  </div>

  <!-- Animations & Blackout — all run locally on ESP, no ArtNet needed -->
  <div class="panel area-rainbow col-right">
    <button class="btn rainbow-btn" id="rainbowBtn" onclick="toggleRainbow()">RAINBOW // ALL HEADS</button>
    <button class="btn demo-btn" id="demoBtn" onclick="toggleDemo()" style="margin-top:6px;width:100%;">◈ DEMO // ALL HEADS</button>
    <button class="btn" id="blackoutBtn" onclick="blackout()" style="margin-top:6px;width:100%;color:var(--danger,#ff4444);border-color:var(--danger,#ff4444);">BLACKOUT</button>
    <div class="motion-row" style="margin-top:10px;">
      <div class="motion-label" style="min-width:54px;font-size:10px;">SPEED</div>
      <input type="range" class="horizontal" min="10" max="300" value="100" step="5" id="fSpeed" oninput="onSpeed()">
      <span id="vSpeed" style="font-family:var(--mono);font-size:11px;color:var(--accent2);min-width:36px;text-align:right;">1.0×</span>
    </div>
  </div>

  <!-- Serial -->
  <div class="panel area-serial col-right">
    <div class="panel-title">// Serial</div><hr>
    <div class="serial-row">
      <input type="text" id="cmdInput" placeholder="R:255,G:0,B:0,W:0,PAN:90,TILT:45">
      <button class="btn primary" onclick="sendRaw()">SEND</button>
    </div>
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
var rainbowActive=false,demoActive=false,seqSelectedIds=[],sendTimer=null,speedTimer=null,_editCueId=null,_cDragId=null;
fetch('/api/status').then(function(r){return r.json();}).then(function(d){
  if(d.ip) document.getElementById('ipLabel').textContent=d.ip;
  if(d.rainbowActive){ rainbowActive=true; document.getElementById('rainbowBtn').classList.add('active'); }
  if(d.demoActive){    demoActive=true;    document.getElementById('demoBtn').classList.add('active'); }
  if(d.animSpeed){
    var sv=Math.round(d.animSpeed*100);
    document.getElementById('fSpeed').value=sv;
    document.getElementById('vSpeed').textContent=(d.animSpeed).toFixed(1)+'×';
  }
});
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
  if(rainbowActive){ demoActive=false; document.getElementById('demoBtn').classList.remove('active'); }
  document.getElementById('rainbowBtn').classList.toggle('active',rainbowActive);
  fetch('/api/rainbow',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:rainbowActive})});
  toast(rainbowActive?'Rainbow ON \u2014 all heads':'Rainbow OFF');
}
function toggleDemo(){
  demoActive=!demoActive;
  if(demoActive){ rainbowActive=false; document.getElementById('rainbowBtn').classList.remove('active'); }
  document.getElementById('demoBtn').classList.toggle('active',demoActive);
  fetch('/api/demo',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:demoActive})});
  toast(demoActive?'Demo ON \u2014 all heads':'Demo OFF');
}
function blackout(){
  rainbowActive=false; demoActive=false;
  document.getElementById('rainbowBtn').classList.remove('active');
  document.getElementById('demoBtn').classList.remove('active');
  fetch('/api/blackout',{method:'POST'});
  toast('Blackout');
}
function onSpeed(){
  var val=+document.getElementById('fSpeed').value;
  document.getElementById('vSpeed').textContent=(val/100).toFixed(1)+'x';
  clearTimeout(speedTimer);
  speedTimer=setTimeout(function(){
    fetch('/api/animation/speed',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({speed:val/100})});
  },40);
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
  fetch(url).then(function(r){
    if(!r.ok){var el=document.getElementById(id);if(el)el.style.display='none';return null;}
    return r.text();
  }).then(function(html){
    if(!html)return;
    var el=document.getElementById(id);if(!el)return;
    el.innerHTML=html;
    el.querySelectorAll('script').forEach(function(old){
      var s=document.createElement('script');s.textContent=old.textContent;
      old.parentNode.replaceChild(s,old);
    });
  }).catch(function(){
    var el=document.getElementById(id);if(el)el.style.display='none';
  });
}
loadModule('/plugins/wifi/discovery_panel.html','module-container');
loadModule('/plugins/artnet/panel.html','artnet-module-container');
loadModule('/plugins/log/panel.html','log-module-container');
var _artnetWasActive=false;
function an_pollStatus(){
  fetch('/api/artnet/status').then(function(r){return r.json();}).then(function(d){
    var active=!!d.active;
    // Toggle bar visibility and control lock only when state changes
    if(active!==_artnetWasActive){
      _artnetWasActive=active;
      document.getElementById('artnet-bar').classList.toggle('visible',active);
      ['.area-light','.area-motion','.area-rainbow','.area-serial','.area-sequencer'].forEach(function(sel){
        var el=document.querySelector(sel);if(el)el.classList.toggle('artnet-locked',active);
      });
    }
    // Always refresh live values while active
    if(active){
      document.getElementById('artnet-live').textContent=
        'R:'+d.r+' G:'+d.g+' B:'+d.b+' W:'+d.w+
        '  PAN:'+d.pan+'\u00b0 TILT:'+d.tilt+'\u00b0';
      document.getElementById('artnet-bar-count').textContent=d.patchCount+' fix';
    } else {
      document.getElementById('artnet-live').textContent='';
      document.getElementById('artnet-bar-count').textContent='';
    }
  }).catch(function(){});
}
setInterval(an_pollStatus,250);
</script>
</body>
</html>
)=====";
