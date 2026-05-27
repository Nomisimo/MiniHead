/* MiniHead PC Leader v2 — app.js */
'use strict';

// ── State ──────────────────────────────────────────────────────────────────
var rainbowActive = false;
var seqSelectedIds = [];
var sendTimer = null;
var _editCueId = null;
var _cDragId = null;
var _artnetWasActive = false;
var LOG_KEYS = ['wifi', 'udp', 'artnet', 'motion', 'serial', 'system'];

// ── Init ───────────────────────────────────────────────────────────────────
try {
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    if(d.ip) document.getElementById('ipLabel').textContent = d.ip;
  }).catch(function(){});
  fetch('/api/version').then(function(r){return r.json();}).then(function(d){
    if(d.version) document.getElementById('appVersion').textContent = '['+d.version+']';
  }).catch(function(){});

  buildLogGrid();
  loadLogFlags();
  updatePreview();
  loadCues();

  var cmdEl = document.getElementById('cmdInput');
  if(cmdEl) cmdEl.addEventListener('keydown', function(e){if(e.key==='Enter')sendRaw();});

  setInterval(an_pollStatus, 2000);
} catch(e) {
  console.error('[init] error:', e);
}

// ── Helpers ────────────────────────────────────────────────────────────────
function toast(msg, type) {
  var el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'show ' + (type || 'ok');
  clearTimeout(el._t);
  el._t = setTimeout(function(){ el.className = ''; }, 2000);
}

function getValues() {
  return {
    r:    +document.getElementById('fR').value,
    g:    +document.getElementById('fG').value,
    b:    +document.getElementById('fB').value,
    w:    +document.getElementById('fW').value,
    pan:  +document.getElementById('fPan').value,
    tilt: +document.getElementById('fTilt').value,
  };
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

// ── Light faders ───────────────────────────────────────────────────────────
function updatePreview() {
  var v = getValues();
  var wr = Math.min(255, v.r+v.w), wg = Math.min(255, v.g+v.w), wb = Math.min(255, v.b+v.w);
  var hex = 'rgb('+wr+','+wg+','+wb+')';
  var br = (wr+wg+wb)/3;
  var p = document.getElementById('ledPreview');
  p.style.background = hex;
  p.style.boxShadow  = br > 10 ? '0 0 '+(20+br/4)+'px '+(8+br/8)+'px '+hex : 'none';
}

function onFader() {
  var v = getValues();
  document.getElementById('vR').value = v.r;
  document.getElementById('vG').value = v.g;
  document.getElementById('vB').value = v.b;
  document.getElementById('vW').value = v.w;
  updatePreview();
  debounceSend();
}

function onMotion() {
  var v = getValues();
  document.getElementById('vPan').value  = v.pan;
  document.getElementById('vTilt').value = v.tilt;
  debounceSend();
}

function debounceSend() {
  clearTimeout(sendTimer);
  sendTimer = setTimeout(sendCurrent, 40);
}

function sendCurrent() {
  var v = getValues();
  var cmd = 'R:'+v.r+',G:'+v.g+',B:'+v.b+',W:'+v.w+',PAN:'+v.pan+',TILT:'+v.tilt;
  var macs = (typeof nh_getSelectedMACs === 'function') ? nh_getSelectedMACs() : [];
  if(macs.length) {
    fetch('/api/send', {method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({command:cmd, targets:macs})});
  } else {
    sendCommand(cmd);
  }
}

function sendCommand(cmd) {
  fetch('/api/send', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({command:cmd})});
}

function sendRaw() {
  var cmd = document.getElementById('cmdInput').value.trim();
  if(!cmd) return;
  sendCommand(cmd);
  toast('Sent: '+cmd);
}

// ── Rainbow ────────────────────────────────────────────────────────────────
function toggleRainbow() {
  rainbowActive = !rainbowActive;
  var btn = document.getElementById('rainbowBtn');
  btn.classList.toggle('active', rainbowActive);
  fetch('/api/rainbow', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({on: rainbowActive})});
  toast(rainbowActive ? 'Rainbow ON — all heads' : 'Rainbow OFF');
}

// ── Cues ───────────────────────────────────────────────────────────────────
function loadCues() {
  fetch('/api/cues').then(function(r){return r.json();}).then(renderCues).catch(function(){});
}

function renderCues(cues) {
  var list = document.getElementById('cueList');
  if(!cues.length) {
    list.innerHTML = '<div style="font-family:var(--mono);font-size:10px;color:var(--text-dim);padding:8px 0;">No cues saved yet</div>';
    return;
  }
  list.innerHTML = '';
  cues.forEach(function(cue) {
    var inSeq = seqSelectedIds.indexOf(cue.id) >= 0;
    var wr = Math.min(255,cue.r+cue.w), wg = Math.min(255,cue.g+cue.w), wb = Math.min(255,cue.b+cue.w);
    var ftStr = (cue.fixTargets&&cue.fixTargets.length)
      ? ' → '+(cue.fixTargets.map(function(id){return id===0?'ALL':'#'+id;}).join(' '))
      : '';
    var el = document.createElement('div');
    el.className = 'cue-item' + (inSeq ? ' seq-selected' : '');
    el.draggable = true;
    el.dataset.cueId = cue.id;
    el.innerHTML =
      '<div class="cue-drag" title="Drag to reorder">&#8942;</div>'+
      '<div class="cue-swatch" style="background:rgb('+wr+','+wg+','+wb+')"></div>'+
      '<div class="cue-info">'+
        '<div class="cue-name">'+escHtml(cue.name)+'</div>'+
        '<div class="cue-meta">P:'+cue.pan+'° T:'+cue.tilt+'° W:'+cue.w+escHtml(ftStr)+'</div>'+
      '</div>'+
      '<div class="cue-actions">'+
        '<button class="icon-btn" title="Edit targets" onclick="editCue('+cue.id+','+JSON.stringify(cue.fixTargets||[0])+')">&#9998;</button>'+
        '<button class="icon-btn" title="Add to sequence" onclick="toggleSeqCue('+cue.id+')">+</button>'+
        '<button class="icon-btn" onclick="fireCue('+cue.id+')">GO</button>'+
        '<button class="icon-btn del" onclick="deleteCue('+cue.id+')">X</button>'+
      '</div>';

    el.addEventListener('dragstart', function(e){
      _cDragId = cue.id; e.dataTransfer.effectAllowed='move'; el.classList.add('dragging');
    });
    el.addEventListener('dragend', function(){
      el.classList.remove('dragging');
      document.querySelectorAll('.cue-item.drag-over').forEach(function(x){x.classList.remove('drag-over');});
    });
    el.addEventListener('dragover', function(e){
      e.preventDefault(); e.dataTransfer.dropEffect='move'; el.classList.add('drag-over');
    });
    el.addEventListener('dragleave', function(e){
      if(!el.contains(e.relatedTarget)) el.classList.remove('drag-over');
    });
    el.addEventListener('drop', function(e){
      e.preventDefault(); el.classList.remove('drag-over');
      if(_cDragId === cue.id) return;
      var items = document.querySelectorAll('.cue-item[data-cue-id]'), order = [];
      items.forEach(function(x){order.push(+x.dataset.cueId);});
      var fi = order.indexOf(_cDragId), ti = order.indexOf(cue.id);
      if(fi<0||ti<0) return;
      order.splice(fi,1); order.splice(ti,0,_cDragId);
      fetch('/api/cues/reorder', {method:'PUT', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({order:order})}).then(function(){loadCues();});
    });
    list.appendChild(el);
  });
}

function saveCue() {
  var name = document.getElementById('cueName').value.trim();
  if(!name) { toast('Enter a cue name', 'err'); return; }
  var v = getValues();
  var ft = (typeof nh_getSelectedFixIDs === 'function') ? nh_getSelectedFixIDs() : [];
  if(!ft.length) ft = [0];
  fetch('/api/cues', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name:name, r:v.r, g:v.g, b:v.b, w:v.w, pan:v.pan, tilt:v.tilt, fixTargets:ft})
  }).then(function(){
    document.getElementById('cueName').value = '';
    toast('Cue saved');
    loadCues();
  });
}

function fireCue(id) {
  fetch('/api/cues/'+id+'/fire', {method:'POST'}).then(function(r){return r.json();}).then(function(d){
    if(d.status==='ok') {
      toast('Cue fired');
      fetch('/api/cues').then(function(r){return r.json();}).then(function(cues){
        var cue = cues.find(function(c){return c.id===id;});
        if(cue) {
          document.getElementById('fR').value    = cue.r;
          document.getElementById('fG').value    = cue.g;
          document.getElementById('fB').value    = cue.b;
          document.getElementById('fW').value    = cue.w;
          document.getElementById('fPan').value  = cue.pan;
          document.getElementById('fTilt').value = cue.tilt;
          onFader(); onMotion();
        }
      });
    }
  }).catch(function(){});
}

function deleteCue(id) {
  seqSelectedIds = seqSelectedIds.filter(function(i){return i!==id;});
  fetch('/api/cues/'+id, {method:'DELETE'}).then(function(){toast('Cue deleted'); loadCues();});
}

function toggleSeqCue(id) {
  var idx = seqSelectedIds.indexOf(id);
  if(idx >= 0) seqSelectedIds.splice(idx,1);
  else seqSelectedIds.push(id);
  loadCues();
  document.getElementById('seqStatus').textContent = seqSelectedIds.length+' cue(s) in sequence';
}

// ── Sequencer ──────────────────────────────────────────────────────────────
function startSequencer() {
  if(!seqSelectedIds.length) { toast('Select cues first', 'err'); return; }
  var interval = +document.getElementById('seqInterval').value;
  var loop     = document.getElementById('seqLoop').checked;
  fetch('/api/sequencer/start', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({cue_ids:seqSelectedIds, interval_ms:interval, loop:loop})
  }).then(function(){
    document.getElementById('seqStatus').textContent = 'Running - '+seqSelectedIds.length+' cues';
    document.getElementById('seqStartBtn').classList.add('active');
    toast('Sequencer started');
  });
}

function stopSequencer() {
  fetch('/api/sequencer/stop', {method:'POST'}).then(function(){
    document.getElementById('seqStatus').textContent = 'Sequencer idle';
    document.getElementById('seqStartBtn').classList.remove('active');
    toast('Sequencer stopped');
  });
}

// ── Cue Edit Modal ─────────────────────────────────────────────────────────
function editCue(id, curTargets) {
  _editCueId = id;
  fetch('/api/fixtures').then(function(r){return r.json();}).catch(function(){return[];}).then(function(fixtures){
    var html = '<label class="modal-check-row"><input type="checkbox" id="cue_all" '+(curTargets.indexOf(0)>=0?'checked':'')+'>'+
               '<span>ALL (broadcast to everyone)</span></label>';
    fixtures.forEach(function(f){
      if(f.id<=0) return;
      var chk = curTargets.indexOf(f.id) >= 0;
      html += '<label class="modal-check-row"><input type="checkbox" class="cue-fix-cb" value="'+f.id+'" '+(chk?'checked':'')+'>'+
              f.id+' — '+escHtml(f.name||'?')+(f.online?' &#9679;':' &#9675;')+'</label>';
    });
    document.getElementById('cueEditList').innerHTML = html;
    var poolIds = fixtures.map(function(f){return f.id;});
    var free = curTargets.filter(function(id){return id>0 && poolIds.indexOf(id)<0;});
    document.getElementById('cueEditFree').value = free.join(', ');
    document.getElementById('cueEditModal').classList.add('open');
  });
}

function cueEditClose() {
  document.getElementById('cueEditModal').classList.remove('open');
  _editCueId = null;
}

function cueEditSave() {
  if(!_editCueId) return;
  var ft = [];
  if(document.getElementById('cue_all').checked) {
    ft = [0];
  } else {
    document.querySelectorAll('.cue-fix-cb:checked').forEach(function(cb){ft.push(parseInt(cb.value));});
    var freeVal = document.getElementById('cueEditFree').value;
    freeVal.split(',').forEach(function(s){var n=parseInt(s.trim());if(n>0&&ft.indexOf(n)<0)ft.push(n);});
  }
  if(!ft.length) ft = [0];
  fetch('/api/cues/'+_editCueId+'/targets', {method:'PUT', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({fixTargets:ft})
  }).then(function(){cueEditClose(); toast('Targets updated'); loadCues();});
}

// ── Art-Net status bar ─────────────────────────────────────────────────────
function an_pollStatus() {
  fetch('/api/artnet/status').then(function(r){return r.json();}).then(function(d){
    var active = !!d.active;
    if(active !== _artnetWasActive) {
      _artnetWasActive = active;
      document.getElementById('artnet-bar').classList.toggle('visible', active);
      ['.area-light','.area-motion','.area-rainbow','.area-serial','.area-sequencer'].forEach(function(sel){
        var el = document.querySelector(sel); if(el) el.classList.toggle('artnet-locked', active);
      });
    }
    if(active) {
      document.getElementById('artnet-live').textContent =
        'R:'+d.r+' G:'+d.g+' B:'+d.b+' W:'+d.w+'  PAN:'+d.pan+'° TILT:'+d.tilt+'°';
      document.getElementById('artnet-bar-count').textContent = d.patchCount+' fix';
    } else {
      document.getElementById('artnet-live').textContent = '';
      document.getElementById('artnet-bar-count').textContent = '';
    }
  }).catch(function(){});
}

// ── Log flags ──────────────────────────────────────────────────────────────
function buildLogGrid() {
  var grid = document.getElementById('logGrid');
  if(!grid) return;
  var html = '';
  LOG_KEYS.forEach(function(k){
    html += '<label class="log-check"><input type="checkbox" id="log_'+k+'" checked><span>'+k+'</span></label>';
  });
  grid.innerHTML = html;
}

function loadLogFlags() {
  fetch('/api/logconfig').then(function(r){return r.json();}).then(function(d){
    LOG_KEYS.forEach(function(k){
      var el = document.getElementById('log_'+k);
      if(el && k in d) el.checked = !!d[k];
    });
  }).catch(function(){});
}

function logSave() {
  var payload = {};
  LOG_KEYS.forEach(function(k){
    var el = document.getElementById('log_'+k);
    if(el) payload[k] = el.checked;
  });
  fetch('/api/esp/logconfig', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify(payload)
  }).then(function(){toast('Log config saved & pushed to ESPs');}).catch(function(){});
}

updatePreview();
loadCues();
