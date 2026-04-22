#pragma once

// ── Discovery Panel HTML Fragment ────────────────────────────────
// Served at GET /plugins/wifi/discovery_panel.html
// Injected into the Future Module Slot by the main page JS.
// Matches PC App discovery_panel.html exactly.
// ─────────────────────────────────────────────────────────────────

const char DISCOVERY_PANEL_HTML[] PROGMEM = R"=====(
<style>
  .heads-toolbar{display:flex;gap:8px;margin-bottom:6px;align-items:center;flex-wrap:wrap;}
  .heads-toolbar.filter-row{margin-bottom:10px;}
  .heads-sel-info{font-family:var(--mono);font-size:10px;color:var(--text-dim);flex:1;min-width:0;}
  .heads-table{width:100%;border-collapse:collapse;font-family:var(--mono);font-size:11px;}
  .heads-table th{text-align:left;color:var(--text-dim);font-weight:normal;padding:4px 6px;border-bottom:1px solid var(--border);letter-spacing:1px;}
  .heads-table td{padding:6px 6px;border-bottom:1px solid rgba(42,42,58,0.5);vertical-align:middle;}
  .heads-table tr:last-child td{border-bottom:none;}
  .heads-table tr.selected-head td{background:rgba(0,229,255,0.05);}
  .fix-id-input{width:40px;background:var(--surface2);border:1px solid var(--border);color:var(--accent2);padding:2px 4px;font-family:var(--mono);font-size:11px;border-radius:3px;text-align:center;-moz-appearance:textfield;}
  .fix-id-input::-webkit-outer-spin-button,.fix-id-input::-webkit-inner-spin-button{-webkit-appearance:none;}
  .fix-id-input.duplicate{border-color:var(--danger)!important;color:var(--danger)!important;}
  .name-input{width:80px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:2px 6px;font-family:var(--mono);font-size:11px;border-radius:3px;}
  .role-badge{padding:1px 5px;border-radius:2px;font-size:9px;letter-spacing:1px;}
  .role-badge.leader{background:rgba(0,229,255,0.15);color:var(--accent);}
  .role-badge.follower{background:rgba(107,107,138,0.2);color:var(--text-dim);}
  .identify-btn{padding:3px 10px;border:1px solid var(--border);background:var(--surface2);color:var(--text-dim);font-family:var(--mono);font-size:10px;border-radius:3px;cursor:pointer;user-select:none;-webkit-user-select:none;touch-action:none;letter-spacing:1px;}
  .identify-btn.holding{border-color:#ffffff;color:#ffffff;background:rgba(255,255,255,0.12);}
  .dup-warn{color:var(--danger);font-size:9px;margin-left:2px;}
  .panel-title-network{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:14px;padding-bottom:10px;border-bottom:1px solid var(--border);}
  .net-btn{padding:6px 12px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:11px;cursor:pointer;border-radius:4px;letter-spacing:1px;transition:all 0.15s;text-transform:uppercase;touch-action:manipulation;}
  .net-btn.primary{border-color:var(--accent);color:var(--accent);background:rgba(0,229,255,0.08);}
  .net-btn.purple{border-color:var(--accent3);color:var(--accent3);background:rgba(168,85,247,0.08);}
  .net-btn.orange{border-color:var(--accent2);color:var(--accent2);background:rgba(255,107,53,0.08);}
  .net-btn.orange.active{background:rgba(255,107,53,0.2);}
  .add-input{background:var(--surface2);border:1px dashed var(--border);color:var(--text);padding:2px 5px;font-family:var(--mono);font-size:11px;border-radius:3px;}

  /* ── Patch bar ── */
  .patch-bar{display:none;align-items:center;gap:10px;flex-wrap:wrap;
    padding:8px 10px;margin-bottom:10px;
    background:rgba(255,107,53,0.06);border:1px solid rgba(255,107,53,0.25);
    border-radius:4px;font-family:var(--mono);font-size:11px;}
  .patch-bar.visible{display:flex;}
  .patch-bar-label{color:var(--accent2);letter-spacing:2px;text-transform:uppercase;font-size:10px;white-space:nowrap;}
  .patch-start{width:44px;background:var(--surface2);border:1px solid rgba(255,107,53,0.4);
    color:var(--accent2);padding:3px 5px;font-family:var(--mono);font-size:12px;
    border-radius:3px;text-align:center;-moz-appearance:textfield;}
  .patch-start::-webkit-outer-spin-button,.patch-start::-webkit-inner-spin-button{-webkit-appearance:none;}
  .patch-next{color:var(--text-dim);font-size:10px;white-space:nowrap;}
  .patch-next strong{color:var(--accent2);font-size:13px;}
  .patch-btn-row{margin-left:auto;display:flex;gap:6px;}

  /* ── Per-row patch assign button ── */
  .patch-assign-btn{
    display:none;padding:2px 10px;border:1px solid var(--accent2);
    background:rgba(255,107,53,0.1);color:var(--accent2);
    font-family:var(--mono);font-size:11px;border-radius:3px;
    cursor:pointer;letter-spacing:1px;white-space:nowrap;
    transition:background 0.1s;
  }
  .patch-assign-btn:hover{background:rgba(255,107,53,0.25);}
  .patch-mode-active .patch-assign-btn{display:inline-block;}
  .patch-mode-active .identify-col{display:none;}
  .patch-patched-flash td{animation:patchFlash 0.4s ease;}
  @keyframes patchFlash{
    0%{background:rgba(255,107,53,0.35);}
    100%{background:transparent;}
  }
</style>

<div class="panel-title-network">// Network Heads</div>

<div class="heads-toolbar">
  <button class="net-btn" onclick="nh_selectAll()">SELECT ALL</button>
  <button class="net-btn" onclick="nh_clearSel()">UNSELECT ALL</button>
  <span class="heads-sel-info" id="nh_selInfo">No heads selected</span>
  <button class="net-btn primary"                     onclick="nh_sendLive()">SEND LIVE</button>
  <button class="net-btn purple"                      onclick="nh_addToCue()">ADD TO CUE &#9660;</button>
  <button class="net-btn orange" id="nh_patchToggle"  onclick="nh_togglePatchMode()">PATCH</button>
</div>
<div class="heads-toolbar filter-row">
  <span style="font-family:var(--mono);font-size:10px;color:var(--text-dim);letter-spacing:1px;">Select:</span>
  <button class="net-btn" style="padding:3px 10px;font-size:10px;" onclick="nh_selectNoFix()">NO FIX#</button>
  <button class="net-btn" style="padding:3px 10px;font-size:10px;" onclick="nh_selectNoDmx()">NO DMX</button>
</div>

<!-- Patch bar — shown in patch mode -->
<div class="patch-bar" id="nh_patchBar">
  <span class="patch-bar-label">// Patch</span>
  <span class="patch-next">Start&nbsp;Fix#</span>
  <input type="number" id="nh_patchStart" class="patch-start" value="1" min="1" max="999"
    oninput="nh_onStartChange()">
  <span class="patch-next">&rarr;&nbsp;Next:&nbsp;<strong id="nh_patchNextLabel">1</strong></span>
  <span class="patch-next" id="nh_patchSelCount" style="color:var(--text-dim);"></span>
  <div class="patch-btn-row">
    <button class="net-btn" style="padding:3px 10px;font-size:10px;" onclick="nh_resetPatchCounter()">RESET</button>
    <button class="net-btn" style="padding:3px 10px;font-size:10px;color:var(--danger);border-color:var(--danger);" onclick="nh_clearFixIDs()">CLEAR FIX IDs</button>
    <button class="net-btn" style="padding:3px 10px;font-size:10px;" onclick="nh_togglePatchMode()">CANCEL</button>
    <button class="net-btn orange" style="padding:3px 10px;font-size:10px;" onclick="nh_patchDone()">DONE</button>
  </div>
</div>

<div style="overflow-x:auto;" id="nh_tableWrap">
  <table class="heads-table">
    <thead>
      <tr>
        <th></th>
        <th>Fix#</th>
        <th>Name</th>
        <th>MAC</th>
        <th>IP</th>
        <th>Role</th>
        <th class="identify-col">Identify</th>
        <th></th><!-- patch button col -->
      </tr>
    </thead>
    <tbody id="nh_tbody">
      <tr><td colspan="8" style="color:var(--text-dim);text-align:center;padding:20px 0;">Scanning...</td></tr>
    </tbody>
  </table>
</div>

<script>
(function(){
  // ── State ────────────────────────────────────────────────────
  var headsData             = [];
  var fixturePool           = [];
  var selectedMACs          = [];
  var selectedOfflineFixIDs = [];
  var identTimers           = {};
  var patchMode             = false;
  var patchNextID           = 1;

  // ── Boot ─────────────────────────────────────────────────────
  nh_load();
  setInterval(nh_load, 2000);

  // ── Load heads + fixture pool ────────────────────────────────
  function nh_load() {
    Promise.all([
      fetch('/api/heads').then(function(r){return r.json();}),
      fetch('/api/fixtures').then(function(r){return r.json();}).catch(function(){return[];})
    ]).then(function(res){
      headsData   = res[0];
      fixturePool = res[1];
      nh_render();
    }).catch(function(){});
  }

  function esc(s){ return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'); }

  // ── Patch mode toggle ────────────────────────────────────────
  window.nh_togglePatchMode = function() {
    patchMode = !patchMode;
    var bar  = document.getElementById('nh_patchBar');
    var btn  = document.getElementById('nh_patchToggle');
    var wrap = document.getElementById('nh_tableWrap');
    if(patchMode){
      bar.classList.add('visible');
      btn.classList.add('active');
      wrap.classList.add('patch-mode-active');
      // Auto-find first unused Fix ID
      var used = headsData.map(function(h){return h.fixID;}).filter(function(id){return id>0;});
      var start = 1;
      while(used.indexOf(start)>=0) start++;
      document.getElementById('nh_patchStart').value = start;
      patchNextID = start;
      nh_updatePatchLabel();
      if(typeof toast==='function') toast('Patch mode \u2014 click a head to assign Fix#');
    } else {
      bar.classList.remove('visible');
      btn.classList.remove('active');
      wrap.classList.remove('patch-mode-active');
    }
    nh_render();
  };

  window.nh_onStartChange = function() {
    var v = parseInt(document.getElementById('nh_patchStart').value)||1;
    patchNextID = Math.max(1, v);
    nh_updatePatchLabel();
  };

  window.nh_resetPatchCounter = function() {
    var v = parseInt(document.getElementById('nh_patchStart').value)||1;
    patchNextID = Math.max(1, v);
    nh_updatePatchLabel();
    if(typeof toast==='function') toast('Counter reset to Fix#'+patchNextID);
  };

  // ── Patch DONE — assign IDs to all selected heads in display order ──
  window.nh_patchDone = function() {
    var sorted = headsData.slice().sort(function(a,b){
      if(a.role==='LEADER'&&b.role!=='LEADER') return -1;
      if(b.role==='LEADER'&&a.role!=='LEADER') return  1;
      if(a.fixID>0&&b.fixID>0) return a.fixID-b.fixID;
      if(a.fixID>0) return -1; if(b.fixID>0) return 1;
      return a.mac.localeCompare(b.mac);
    }).filter(function(h){ return selectedMACs.indexOf(h.mac)>=0; });

    if(!sorted.length){ nh_togglePatchMode(); return; }

    var startId = patchNextID;
    var pending = sorted.length;
    sorted.forEach(function(h, i){
      var id   = startId + i;
      var name = h.name || ('Fix #'+id);
      fetch('/api/heads/'+h.mac+'/fixid',{
        method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({fixID:id})
      }).then(function(){
        h.fixID = id;
        fetch('/api/fixtures',{method:'POST',headers:{'Content-Type':'application/json'},
          body:JSON.stringify({id:id,name:name,mac:h.mac})
        }).catch(function(){
          fetch('/api/fixtures/'+id,{method:'PUT',headers:{'Content-Type':'application/json'},
            body:JSON.stringify({mac:h.mac})});
        });
        pending--;
        if(pending===0){
          patchNextID = startId + sorted.length;
          nh_updatePatchLabel();
          nh_togglePatchMode();
          nh_load();
          if(typeof toast==='function') toast('Patched '+sorted.length+' head(s): Fix#'+startId+'\u2013Fix#'+(startId+sorted.length-1));
        }
      });
    });
  };

  // ── Assign next Fix ID to a single head ──────────────────────
  window.nh_patchAssign = function(mac) {
    var id = patchNextID;
    fetch('/api/heads/'+mac+'/fixid',{
      method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({fixID:id})
    }).then(function(){
      headsData.forEach(function(h){if(h.mac===mac)h.fixID=id;});
      var name = (headsData.find(function(h){return h.mac===mac;})||{}).name || ('Fix #'+id);
      fetch('/api/fixtures',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({id:id,name:name,mac:mac})
      }).catch(function(){
        fetch('/api/fixtures/'+id,{method:'PUT',headers:{'Content-Type':'application/json'},
          body:JSON.stringify({mac:mac})});
      });
      patchNextID++;
      nh_updatePatchLabel();
      nh_render();
      var row = document.querySelector('tr[data-mac="'+mac+'"]');
      if(row){ row.classList.add('patch-patched-flash'); setTimeout(function(){row.classList.remove('patch-patched-flash');},500); }
      if(typeof toast==='function') toast('Fix#'+id+' \u2192 \u2026'+mac.slice(-8)+' \u2014 next: Fix#'+patchNextID);
    });
  };

  function nh_updatePatchLabel() {
    var el = document.getElementById('nh_patchNextLabel');
    if(el) el.textContent = patchNextID;
    var selEl = document.getElementById('nh_patchSelCount');
    if(selEl){
      var n = selectedMACs.length;
      selEl.textContent = n ? '('+n+' selected \u2192 Fix#'+patchNextID+'\u2013Fix#'+(patchNextID+n-1)+')' : '';
    }
  }

  // ── Render ───────────────────────────────────────────────────
  function nh_render() {
    var tbody = document.getElementById('nh_tbody');
    if(!patchMode){
      var active = document.activeElement;
      if (active && tbody && tbody.contains(active)) return;
    }
    tbody.innerHTML = '';

    if (!headsData || !headsData.length) {
      var empty = document.createElement('tr');
      empty.innerHTML = '<td colspan="8" style="color:var(--text-dim);text-align:center;padding:20px 0;">No heads found</td>';
      tbody.appendChild(empty);
    } else {
      var fixCount = {};
      headsData.forEach(function(h){ if(h.fixID>0) fixCount[h.fixID]=(fixCount[h.fixID]||0)+1; });
      headsData.sort(function(a,b){
        if(a.role==='LEADER'&&b.role!=='LEADER') return -1;
        if(b.role==='LEADER'&&a.role!=='LEADER') return  1;
        if(a.fixID>0&&b.fixID>0) return a.fixID-b.fixID;
        if(a.fixID>0) return -1; if(b.fixID>0) return 1;
        return a.mac.localeCompare(b.mac);
      });
      headsData.forEach(function(h){
        var sel = selectedMACs.indexOf(h.mac)>=0;
        var dup = h.fixID>0 && fixCount[h.fixID]>1;
        var tr  = document.createElement('tr');
        tr.dataset.mac = h.mac;
        if(sel) tr.className='selected-head';
        tr.innerHTML=
          '<td><input type="checkbox"'+(sel?' checked':'')+
              ' onchange="nh_toggleSel(\''+h.mac+'\',this.checked)"></td>'+
          '<td><input class="fix-id-input'+(dup?' duplicate':'')+'" type="number" min="1" max="999"'+
              ' value="'+(h.fixID>0?h.fixID:'')+'" placeholder="-"'+
              (dup?' title="WARNING: Duplicate FixID"':'')+
              ' onchange="nh_setFixID(\''+h.mac+'\',this.value)">'+(dup?'<span class="dup-warn">DUP</span>':'')+
          '</td>'+
          '<td><input class="name-input" type="text" value="'+esc(h.name||'')+'"'+
              ' placeholder="Name..." onchange="nh_setName(\''+h.mac+'\',this.value)"></td>'+
          '<td style="color:var(--text-dim);" title="'+h.mac+'">\u2026'+h.mac.slice(-8)+'</td>'+
          '<td style="color:var(--accent);">'+h.ip+'</td>'+
          '<td><span class="role-badge '+(h.role==='LEADER'?'leader':'follower')+'">'+h.role+'</span></td>'+
          '<td class="identify-col"><button class="identify-btn" data-mac="'+h.mac+'"'+
            ' onmousedown="nh_identStart(\''+h.mac+'\')"'+
            ' onmouseup="nh_identStop(\''+h.mac+'\')"'+
            ' onmouseleave="nh_identStop(\''+h.mac+'\')"'+
            ' ontouchstart="nh_identStart(\''+h.mac+'\')"'+
            ' ontouchend="nh_identStop(\''+h.mac+'\')">HOLD</button></td>'+
          '<td><button class="patch-assign-btn" onclick="nh_patchAssign(\''+h.mac+'\')">\u2192 Fix#'+patchNextID+'</button></td>';
        tbody.appendChild(tr);
      });
    }

    // ── Offline section ───────────────────────────────────────
    var onlineFixIDs = headsData.map(function(h){return h.fixID;});
    var onlineMACs   = headsData.map(function(h){return h.mac;});
    var offline = fixturePool.filter(function(f){
      if(f.mac && onlineMACs.indexOf(f.mac)>=0) return false;
      if(f.id>0 && onlineFixIDs.indexOf(f.id)>=0) return false;
      return true;
    });
    if(offline.length>0){
      var sep=document.createElement('tr');
      sep.innerHTML='<td colspan="8" style="text-align:center;color:var(--text-dim);font-size:9px;letter-spacing:2px;padding:5px 0;border-top:1px solid var(--border);">\u2500\u2500 OFFLINE \u2500\u2500</td>';
      tbody.appendChild(sep);
      offline.forEach(function(f){
        var sel=selectedOfflineFixIDs.indexOf(f.id)>=0;
        var tr=document.createElement('tr');
        tr.style.opacity='0.55';
        tr.innerHTML=
          '<td><input type="checkbox"'+(sel?' checked':'')+
              ' onchange="nh_toggleOffline('+f.id+',this.checked)"></td>'+
          '<td><span style="color:var(--accent2);font-family:var(--mono);font-size:11px;">'+(f.id>0?f.id:'\u2014')+'</span></td>'+
          '<td><input class="name-input" type="text" value="'+esc(f.name||'')+'"'+
              ' placeholder="Name..." onchange="nh_renameFixture('+f.id+',this.value)"></td>'+
          '<td style="color:var(--text-dim);">'+(f.mac?('\u2026'+f.mac.slice(-8)):'\u2014')+'</td>'+
          '<td>\u2014</td>'+
          '<td><span class="role-badge" style="background:rgba(107,107,138,0.15);color:var(--text-dim)">OFFLINE</span></td>'+
          '<td class="identify-col"><button class="identify-btn" onclick="nh_deleteFixture('+f.id+')" title="Remove" style="color:var(--danger);border-color:var(--danger);">\u2715</button></td>'+
          '<td></td>';
        tbody.appendChild(tr);
      });
    }

    // ── Add row ───────────────────────────────────────────────
    var addRow=document.createElement('tr');
    addRow.innerHTML=
      '<td></td>'+
      '<td><input type="number" id="nh_newId" min="1" max="999" placeholder="ID"'+
          ' class="add-input" style="width:38px;color:var(--accent2);text-align:center;"></td>'+
      '<td><input type="text" id="nh_newName" placeholder="Name..."'+
          ' class="add-input" style="width:78px;"></td>'+
      '<td><input type="text" id="nh_newMac" placeholder="MAC (opt)"'+
          ' class="add-input" style="width:88px;font-size:10px;color:var(--text-dim);"></td>'+
      '<td colspan="2"></td>'+
      '<td class="identify-col"><button class="identify-btn" onclick="nh_addFixture()" style="color:var(--success);border-color:var(--success);">+</button></td>'+
      '<td></td>';
    tbody.appendChild(addRow);

    // Update all assign-btn labels in patch mode
    if(patchMode){
      tbody.querySelectorAll('.patch-assign-btn').forEach(function(btn){
        btn.textContent = '\u2192 Fix#'+patchNextID;
      });
    }

    nh_updateSelInfo();
    nh_updatePatchLabel();
  }

  // ── Selection ─────────────────────────────────────────────────
  window.nh_toggleSel = function(mac, checked) {
    if(checked){ if(selectedMACs.indexOf(mac)<0) selectedMACs.push(mac); }
    else        { selectedMACs=selectedMACs.filter(function(m){return m!==mac;}); }
    nh_render();
  };
  window.nh_toggleOffline = function(fixId, checked) {
    if(checked){ if(selectedOfflineFixIDs.indexOf(fixId)<0) selectedOfflineFixIDs.push(fixId); }
    else        { selectedOfflineFixIDs=selectedOfflineFixIDs.filter(function(i){return i!==fixId;}); }
    nh_render();
  };
  window.nh_selectAll = function() {
    selectedMACs=headsData.map(function(h){return h.mac;});
    var onFix=headsData.map(function(h){return h.fixID;});
    var onMac=headsData.map(function(h){return h.mac;});
    selectedOfflineFixIDs=fixturePool.filter(function(f){
      return !(f.mac&&onMac.indexOf(f.mac)>=0)&&!(f.id>0&&onFix.indexOf(f.id)>=0);
    }).map(function(f){return f.id;});
    nh_render();
  };
  window.nh_clearSel = function() {
    selectedMACs=[]; selectedOfflineFixIDs=[];
    nh_render();
  };

  // ── Select: no Fix# ──────────────────────────────────────────
  window.nh_selectNoFix = function() {
    selectedMACs = headsData.filter(function(h){ return !(h.fixID>0); }).map(function(h){ return h.mac; });
    selectedOfflineFixIDs = [];
    nh_render();
    if(typeof toast==='function') toast(selectedMACs.length+' head(s) without Fix#');
  };

  // ── Select: no DMX patch ─────────────────────────────────────
  window.nh_selectNoDmx = function() {
    fetch('/api/artnet/patch').then(function(r){return r.json();}).then(function(patches){
      var patchedIDs = patches.map(function(p){return p.fixID;});
      selectedMACs = headsData.filter(function(h){
        return !(h.fixID>0) || patchedIDs.indexOf(h.fixID)<0;
      }).map(function(h){ return h.mac; });
      selectedOfflineFixIDs = [];
      nh_render();
      if(typeof toast==='function') toast(selectedMACs.length+' head(s) without DMX patch');
    }).catch(function(){
      if(typeof toast==='function') toast('Could not fetch Art-Net patches','err');
    });
  };

  // ── Clear Fix IDs from selected heads ────────────────────────
  window.nh_clearFixIDs = function() {
    var targets = selectedMACs.slice();
    if(!targets.length){ if(typeof toast==='function') toast('No heads selected','err'); return; }
    var pending = targets.length;
    targets.forEach(function(mac){
      fetch('/api/heads/'+mac+'/fixid',{
        method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({fixID:0})
      }).then(function(){
        headsData.forEach(function(h){ if(h.mac===mac) h.fixID=0; });
        if(--pending===0){
          if(typeof toast==='function') toast('Cleared Fix IDs on '+targets.length+' head(s)');
          nh_load();
        }
      });
    });
  };

  function nh_updateSelInfo() {
    var el=document.getElementById('nh_selInfo');
    var labels=selectedMACs.map(function(mac){
      var h=headsData.find(function(x){return x.mac===mac;});
      return (h&&h.fixID>0)?'Fix#'+h.fixID:'\u2026'+mac.slice(-5);
    });
    selectedOfflineFixIDs.forEach(function(id){labels.push('Fix#'+id+'(off)');});
    el.textContent=labels.length?labels.join(', '):'No heads selected';
  }

  // ── Fix ID ───────────────────────────────────────────────────
  window.nh_setFixID = function(mac, val) {
    var id=parseInt(val)||0;
    fetch('/api/heads/'+mac+'/fixid',{
      method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({fixID:id})
    }).then(function(){
      headsData.forEach(function(h){if(h.mac===mac)h.fixID=id;});
      nh_render();
      if(typeof toast==='function') toast('Fix#'+id+' saved');
    });
  };

  // ── Name ─────────────────────────────────────────────────────
  window.nh_setName = function(mac, name) {
    fetch('/api/heads/'+mac+'/name',{
      method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({name:name})
    }).then(function(){
      headsData.forEach(function(h){if(h.mac===mac)h.name=name;});
      if(typeof toast==='function') toast('Name saved');
    });
  };

  // ── Fixture pool ──────────────────────────────────────────────
  window.nh_renameFixture = function(id, name) {
    fetch('/api/fixtures/'+id,{
      method:'PUT',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({name:name})
    }).then(function(){
      fixturePool.forEach(function(f){if(f.id===id)f.name=name;});
      if(typeof toast==='function') toast('Fixture renamed');
    });
  };
  window.nh_addFixture = function() {
    var idEl=document.getElementById('nh_newId');
    var nameEl=document.getElementById('nh_newName');
    var macEl=document.getElementById('nh_newMac');
    var id=parseInt(idEl.value)||0;
    if(id<=0){if(typeof toast==='function') toast('Enter a valid Fix# ID','err');return;}
    fetch('/api/fixtures',{
      method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({id:id,name:nameEl.value.trim(),mac:macEl.value.trim()||null})
    }).then(function(r){return r.json();}).then(function(d){
      if(d.status==='ok'){
        idEl.value='';nameEl.value='';macEl.value='';
        if(typeof toast==='function') toast('Fixture #'+id+' added');
        nh_load();
      } else {
        if(typeof toast==='function') toast(d.message||'Error','err');
      }
    });
  };
  window.nh_deleteFixture = function(id) {
    fetch('/api/fixtures/'+id,{method:'DELETE'}).then(function(){
      fixturePool=fixturePool.filter(function(f){return f.id!==id;});
      selectedOfflineFixIDs=selectedOfflineFixIDs.filter(function(i){return i!==id;});
      nh_render();
      if(typeof toast==='function') toast('Fixture removed');
    });
  };

  // ── Send live ────────────────────────────────────────────────
  window.nh_sendLive = function() {
    if(!selectedMACs.length){if(typeof toast==='function') toast('Select heads first','err');return;}
    var v=(typeof getValues==='function')?getValues():null;
    if(!v){if(typeof toast==='function') toast('Faders not ready','err');return;}
    var cmd='R:'+v.r+',G:'+v.g+',B:'+v.b+',W:'+v.w+',PAN:'+v.pan+',TILT:'+v.tilt;
    fetch('/api/send',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({command:cmd,targets:selectedMACs.slice()})});
    if(typeof toast==='function') toast('Sent to '+selectedMACs.length+' head(s)');
  };

  // ── Add to cue ───────────────────────────────────────────────
  window.nh_addToCue = function() {
    if(!selectedMACs.length&&!selectedOfflineFixIDs.length){
      if(typeof toast==='function') toast('Select heads first','err');return;
    }
    var nameEl=document.getElementById('cueName');
    var name=nameEl?nameEl.value.trim():'';
    if(!name){if(typeof toast==='function') toast('Enter cue name first','err');return;}
    var v=(typeof getValues==='function')?getValues():{r:0,g:0,b:0,w:0,pan:90,tilt:90};
    var fixIds=selectedMACs.map(function(mac){
      var h=headsData.find(function(x){return x.mac===mac;});
      return h?h.fixID:0;
    }).filter(function(id){return id>0;});
    fixIds=fixIds.concat(selectedOfflineFixIDs.filter(function(id){return fixIds.indexOf(id)<0;}));
    if(!fixIds.length) fixIds=[0];
    fetch('/api/cues',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({name:name,r:v.r,g:v.g,b:v.b,w:v.w,pan:v.pan,tilt:v.tilt,fixTargets:fixIds})
    }).then(function(){
      if(nameEl) nameEl.value='';
      var tStr=fixIds.map(function(id){return id===0?'ALL':'Fix#'+id;}).join(', ');
      if(typeof toast==='function') toast('Cue saved \u2192 '+tStr);
      if(typeof loadCues==='function') loadCues();
    });
  };

  // ── Identify ─────────────────────────────────────────────────
  window.nh_identStart = function(mac) {
    if(identTimers[mac]) return;
    nh_identSend(mac,true);
    identTimers[mac]=setInterval(function(){nh_identSend(mac,true);},500);
    var btn=document.querySelector('.identify-btn[data-mac="'+mac+'"]');
    if(btn) btn.classList.add('holding');
  };
  window.nh_identStop = function(mac) {
    if(!identTimers[mac]) return;
    clearInterval(identTimers[mac]); delete identTimers[mac];
    nh_identSend(mac,false);
    var btn=document.querySelector('.identify-btn[data-mac="'+mac+'"]');
    if(btn) btn.classList.remove('holding');
  };
  function nh_identSend(mac,on){
    fetch('/api/heads/'+mac+'/identify',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({on:on})});
  }

  // ── Expose to main page ───────────────────────────────────────
  window.nh_getSelectedMACs   = function(){ return selectedMACs.slice(); };
  window.nh_getSelectedFixIDs = function(){
    var online=selectedMACs.map(function(mac){
      var h=headsData.find(function(x){return x.mac===mac;});
      return h?h.fixID:0;
    }).filter(function(id){return id>0;});
    return online.concat(selectedOfflineFixIDs.filter(function(id){return online.indexOf(id)<0;}));
  };

})(); // end IIFE
</script>
)=====";
