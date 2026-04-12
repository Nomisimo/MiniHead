#pragma once

// ── Discovery Panel HTML Fragment ────────────────────────────────
// Served at GET /modules/discovery/discovery_panel.html
// Injected into the Future Module Slot by the main page JS
// ─────────────────────────────────────────────────────────────────

const char DISCOVERY_PANEL_HTML[] PROGMEM = R"=====(
<style>
  .heads-toolbar{display:flex;gap:8px;margin-bottom:12px;align-items:center;flex-wrap:wrap;}
  .heads-sel-info{font-family:var(--mono);font-size:10px;color:var(--text-dim);flex:1;min-width:0;}
  .heads-table{width:100%;border-collapse:collapse;font-family:var(--mono);font-size:11px;}
  .heads-table th{text-align:left;color:var(--text-dim);font-weight:normal;padding:4px 6px;border-bottom:1px solid var(--border);letter-spacing:1px;}
  .heads-table td{padding:6px 6px;border-bottom:1px solid rgba(42,42,58,0.5);vertical-align:middle;}
  .heads-table tr:last-child td{border-bottom:none;}
  .heads-table tr.selected-head td{background:rgba(0,229,255,0.05);}
  .fix-id-input{width:40px;background:var(--surface2);border:1px solid var(--border);color:var(--accent2);padding:2px 4px;font-family:var(--mono);font-size:11px;border-radius:3px;text-align:center;-moz-appearance:textfield;}
  .fix-id-input::-webkit-outer-spin-button,.fix-id-input::-webkit-inner-spin-button{-webkit-appearance:none;}
  .fix-id-input.duplicate{border-color:var(--danger)!important;color:var(--danger)!important;}
  .role-badge{padding:1px 5px;border-radius:2px;font-size:9px;letter-spacing:1px;}
  .role-badge.leader{background:rgba(0,229,255,0.15);color:var(--accent);}
  .role-badge.follower{background:rgba(107,107,138,0.2);color:var(--text-dim);}
  .identify-btn{padding:3px 10px;border:1px solid var(--border);background:var(--surface2);color:var(--text-dim);font-family:var(--mono);font-size:10px;border-radius:3px;cursor:pointer;user-select:none;-webkit-user-select:none;touch-action:none;letter-spacing:1px;}
  .identify-btn.holding{border-color:#ffffff;color:#ffffff;background:rgba(255,255,255,0.12);}
  .dup-warn{color:var(--danger);font-size:9px;margin-left:2px;}
  .panel-title-network{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:16px;padding-bottom:10px;border-bottom:1px solid var(--border);}
  .net-btn{padding:6px 12px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:11px;cursor:pointer;border-radius:4px;letter-spacing:1px;transition:all 0.15s;text-transform:uppercase;touch-action:manipulation;}
  .net-btn.primary{border-color:var(--accent);color:var(--accent);background:rgba(0,229,255,0.08);}
  .net-btn.purple{border-color:var(--accent3);color:var(--accent3);background:rgba(168,85,247,0.08);}
</style>

<div class="panel-title-network">// Network Heads</div>

<div class="heads-toolbar">
  <button class="net-btn" onclick="nh_selectAll()">SELECT ALL</button>
  <button class="net-btn" onclick="nh_clearSel()">CLEAR</button>
  <span class="heads-sel-info" id="nh_selInfo">No heads selected</span>
  <button class="net-btn primary" onclick="nh_sendLive()">SEND LIVE</button>
  <button class="net-btn purple"  onclick="nh_addToCue()">ADD TO CUE ▾</button>
</div>

<div style="overflow-x:auto;">
  <table class="heads-table">
    <thead>
      <tr>
        <th></th>
        <th>Fix#</th>
        <th>MAC</th>
        <th>IP</th>
        <th>Role</th>
        <th>Identify</th>
      </tr>
    </thead>
    <tbody id="nh_tbody">
      <tr><td colspan="6" style="color:var(--text-dim);text-align:center;padding:20px 0;">Scanning...</td></tr>
    </tbody>
  </table>
</div>

<script>
(function(){
  // ── State ────────────────────────────────────────────────────
  var headsData    = [];
  var selectedMACs = [];
  var identTimers  = {};  // mac -> setInterval id

  // ── Boot: start polling ──────────────────────────────────────
  nh_load();
  setInterval(nh_load, 2000);

  // ── Load heads from leader API ───────────────────────────────
  function nh_load() {
    fetch('/api/heads')
      .then(function(r){ return r.json(); })
      .then(function(data){ headsData = data; nh_render(data); })
      .catch(function(){});
  }

  // ── Render table ─────────────────────────────────────────────
  function nh_render(heads) {
    var tbody = document.getElementById('nh_tbody');
    if (!heads || !heads.length) {
      tbody.innerHTML = '<tr><td colspan="6" style="color:var(--text-dim);text-align:center;padding:20px 0;">No heads found</td></tr>';
      return;
    }

    // Count fix IDs to detect duplicates
    var fixCount = {};
    heads.forEach(function(h){ if(h.fixID>0) fixCount[h.fixID]=(fixCount[h.fixID]||0)+1; });

    // Sort: leader first, then by fixID asc (0 last), then MAC
    heads.sort(function(a,b){
      if(a.role==='LEADER' && b.role!=='LEADER') return -1;
      if(b.role==='LEADER' && a.role!=='LEADER') return  1;
      if(a.fixID>0 && b.fixID>0) return a.fixID - b.fixID;
      if(a.fixID>0) return -1;
      if(b.fixID>0) return  1;
      return a.mac.localeCompare(b.mac);
    });

    tbody.innerHTML = '';
    heads.forEach(function(h) {
      var sel  = selectedMACs.indexOf(h.mac) >= 0;
      var dup  = h.fixID > 0 && fixCount[h.fixID] > 1;
      var tr   = document.createElement('tr');
      if (sel) tr.className = 'selected-head';

      tr.innerHTML =
        '<td><input type="checkbox"'+(sel?' checked':'')+
            ' onchange="nh_toggleSel(\''+h.mac+'\',this.checked)"></td>'+
        '<td>'+
          '<input class="fix-id-input'+(dup?' duplicate':'')+'" type="number" min="1" max="999"'+
              ' value="'+(h.fixID>0?h.fixID:'')+'" placeholder="-"'+
              ' title="'+(dup?'WARNING: Duplicate FixID':'')+'"'+
              ' onchange="nh_setFixID(\''+h.mac+'\',this.value)">'+
          (dup?'<span class="dup-warn">DUP</span>':'')+
        '</td>'+
        '<td style="color:var(--text-dim);" title="'+h.mac+'">…'+h.mac.slice(-8)+'</td>'+
        '<td style="color:var(--accent);">'+h.ip+'</td>'+
        '<td><span class="role-badge '+(h.role==='LEADER'?'leader':'follower')+'">'+h.role+'</span></td>'+
        '<td>'+
          '<button class="identify-btn" data-mac="'+h.mac+'"'+
            ' onmousedown="nh_identStart(\''+h.mac+'\')"'+
            ' onmouseup="nh_identStop(\''+h.mac+'\')"'+
            ' onmouseleave="nh_identStop(\''+h.mac+'\')"'+
            ' ontouchstart="nh_identStart(\''+h.mac+'\')"'+
            ' ontouchend="nh_identStop(\''+h.mac+'\')">HOLD</button>'+
        '</td>';
      tbody.appendChild(tr);
    });
    nh_updateSelInfo();
  }

  // ── Selection ─────────────────────────────────────────────────
  window.nh_toggleSel = function(mac, checked) {
    if(checked){ if(selectedMACs.indexOf(mac)<0) selectedMACs.push(mac); }
    else        { selectedMACs = selectedMACs.filter(function(m){return m!==mac;}); }
    nh_render(headsData);
  };
  window.nh_selectAll = function() {
    selectedMACs = headsData.map(function(h){return h.mac;});
    nh_render(headsData);
  };
  window.nh_clearSel = function() {
    selectedMACs = [];
    nh_render(headsData);
  };
  function nh_updateSelInfo() {
    var el = document.getElementById('nh_selInfo');
    if (!selectedMACs.length){ el.textContent='No heads selected'; return; }
    var labels = selectedMACs.map(function(mac){
      var h = headsData.find(function(x){return x.mac===mac;});
      return (h && h.fixID>0) ? 'Fix#'+h.fixID : '…'+mac.slice(-5);
    });
    el.textContent = labels.join(', ');
  }

  // ── Fix ID assignment ─────────────────────────────────────────
  window.nh_setFixID = function(mac, val) {
    var id = parseInt(val)||0;
    fetch('/api/heads/'+mac+'/fixid', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({fixID:id})
    }).then(function(){
      headsData.forEach(function(h){ if(h.mac===mac) h.fixID=id; });
      nh_render(headsData);
      if(typeof toast==='function') toast('Fix#'+id+' saved');
    });
  };

  // ── Send live to selected ─────────────────────────────────────
  window.nh_sendLive = function() {
    if(!selectedMACs.length){ if(typeof toast==='function') toast('Select heads first','err'); return; }
    var v = (typeof getValues==='function') ? getValues() : null;
    if(!v){ if(typeof toast==='function') toast('Faders not ready','err'); return; }
    var cmd = 'R:'+v.r+',G:'+v.g+',B:'+v.b+',W:'+v.w+',PAN:'+v.pan+',TILT:'+v.tilt;
    fetch('/api/send',{
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({command:cmd, targets:selectedMACs.slice()})
    });
    if(typeof toast==='function') toast('Sent to '+selectedMACs.length+' head(s)');
  };

  // ── Add selected to cue ───────────────────────────────────────
  window.nh_addToCue = function() {
    if(!selectedMACs.length){ if(typeof toast==='function') toast('Select heads first','err'); return; }
    var nameEl = document.getElementById('cueName');
    var name   = nameEl ? nameEl.value.trim() : '';
    if(!name){ if(typeof toast==='function') toast('Enter cue name first','err'); return; }
    var v = (typeof getValues==='function') ? getValues() : {r:0,g:0,b:0,w:0,pan:90,tilt:90};
    fetch('/api/cues',{
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({name:name, r:v.r, g:v.g, b:v.b, w:v.w,
                            pan:v.pan, tilt:v.tilt, targets:selectedMACs.slice()})
    }).then(function(){
      if(nameEl) nameEl.value='';
      if(typeof toast==='function')    toast('Cue saved with '+selectedMACs.length+' target(s)');
      if(typeof loadCues==='function') loadCues();
    });
  };

  // ── Identify: hold = on, release = off ───────────────────────
  window.nh_identStart = function(mac) {
    if(identTimers[mac]) return;
    nh_identSend(mac, true);
    identTimers[mac] = setInterval(function(){ nh_identSend(mac, true); }, 500);
    var btn = document.querySelector('.identify-btn[data-mac="'+mac+'"]');
    if(btn) btn.classList.add('holding');
  };
  window.nh_identStop = function(mac) {
    if(!identTimers[mac]) return;
    clearInterval(identTimers[mac]);
    delete identTimers[mac];
    nh_identSend(mac, false);
    var btn = document.querySelector('.identify-btn[data-mac="'+mac+'"]');
    if(btn) btn.classList.remove('holding');
  };
  function nh_identSend(mac, on) {
    fetch('/api/heads/'+mac+'/identify',{
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({on:on})
    });
  }

})(); // end IIFE — no global namespace pollution
</script>
)=====";