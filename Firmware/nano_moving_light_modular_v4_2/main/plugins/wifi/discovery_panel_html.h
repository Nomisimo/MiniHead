#pragma once

// ── Discovery Panel HTML Fragment ────────────────────────────────
// Served at GET /plugins/wifi/discovery_panel.html
// Injected into the Future Module Slot by the main page JS
// ─────────────────────────────────────────────────────────────────

const char DISCOVERY_PANEL_HTML[] PROGMEM = R"=====(
<style>
  .heads-toolbar{display:flex;gap:8px;margin-bottom:8px;align-items:center;flex-wrap:wrap;}
  .heads-toolbar.filter-row{margin-bottom:10px;}
  .heads-sel-info{font-family:var(--mono);font-size:10px;color:var(--text-dim);flex:1;min-width:0;}
  .heads-table{width:100%;border-collapse:collapse;font-family:var(--mono);font-size:11px;}
  .heads-table th{text-align:left;color:var(--text-dim);font-weight:normal;padding:4px 6px;border-bottom:1px solid var(--border);letter-spacing:1px;font-size:10px;}
  .heads-table td{padding:5px 6px;border-bottom:1px solid rgba(42,42,58,0.5);vertical-align:middle;}
  .heads-table tr:last-child td{border-bottom:none;}
  .heads-table tr.selected-head td{background:rgba(0,229,255,0.05);}
  .fix-id-input{width:40px;background:var(--surface2);border:1px solid var(--border);color:var(--accent2);padding:2px 4px;font-family:var(--mono);font-size:11px;border-radius:3px;text-align:center;-moz-appearance:textfield;}
  .fix-id-input::-webkit-outer-spin-button,.fix-id-input::-webkit-inner-spin-button{-webkit-appearance:none;}
  .fix-id-input.duplicate{border-color:var(--danger)!important;color:var(--danger)!important;}
  .patch-num{width:42px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:2px 3px;font-family:var(--mono);font-size:10px;border-radius:3px;text-align:center;-moz-appearance:textfield;}
  .patch-num::-webkit-outer-spin-button,.patch-num::-webkit-inner-spin-button{-webkit-appearance:none;}
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
  .net-btn.orange{border-color:#f97316;color:#f97316;background:rgba(249,115,22,0.08);}
  .net-btn.sm{padding:3px 10px;font-size:10px;}
  .add-input{background:var(--surface2);border:1px dashed var(--border);color:var(--text);padding:2px 5px;font-family:var(--mono);font-size:11px;border-radius:3px;}
  /* ── Patch bar ── */
  .patch-bar{background:rgba(249,115,22,0.06);border:1px solid rgba(249,115,22,0.3);border-radius:4px;padding:10px 12px;margin-bottom:10px;display:none;}
  .patch-bar.visible{display:block;}
  .patch-bar-label{font-family:var(--mono);font-size:10px;color:#f97316;letter-spacing:2px;}
  .patch-bar-row{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-top:8px;}
  .patch-next{font-family:var(--mono);font-size:11px;color:var(--text-dim);}
  .patch-start-input{width:52px;background:var(--surface2);border:1px solid rgba(249,115,22,0.5);color:#f97316;padding:4px 6px;font-family:var(--mono);font-size:13px;border-radius:3px;text-align:center;-moz-appearance:textfield;}
  .patch-start-input::-webkit-outer-spin-button,.patch-start-input::-webkit-inner-spin-button{-webkit-appearance:none;}
  .patch-btn-row{display:flex;gap:6px;flex-wrap:wrap;margin-top:8px;}
  .patch-sel-count{font-family:var(--mono);font-size:10px;color:var(--text-dim);margin-top:4px;}
</style>

<div class="panel-title-network">// Network Heads</div>

<!-- Toolbar row 1 -->
<div class="heads-toolbar">
  <button class="net-btn sm" onclick="nh_selectAll()">SELECT ALL</button>
  <button class="net-btn sm" onclick="nh_clearSel()">UNSELECT ALL</button>
  <span class="heads-sel-info" id="nh_selInfo">No heads selected</span>
  <button class="net-btn sm primary" onclick="nh_sendLive()">SEND LIVE</button>
  <button class="net-btn sm purple" onclick="nh_addToCue()">ADD TO CUE &#9660;</button>
  <button class="net-btn sm orange" id="nh_patchToggle" onclick="nh_togglePatchMode()">SET FIX IDs</button>
</div>

<!-- Toolbar row 2: filters -->
<div class="heads-toolbar filter-row">
  <span style="font-family:var(--mono);font-size:10px;color:var(--text-dim);">Select:</span>
  <button class="net-btn sm" onclick="nh_selectNoFix()">NO FIX#</button>
  <button class="net-btn sm" onclick="nh_selectNoDmx()">NO DMX</button>
</div>

<!-- Patch bar (shown in patch mode) -->
<div class="patch-bar" id="nh_patchBar">
  <span class="patch-bar-label">// SET FIX IDs MODE</span>
  <div class="patch-bar-row">
    <span class="patch-next">Start&nbsp;Fix#</span>
    <input type="number" id="nh_patchStart" class="patch-start-input" value="1" min="1" max="999" oninput="nh_resetPatchCounter()">
    <span class="patch-next">&rarr;&nbsp;Next:&nbsp;<strong id="nh_patchNextLabel" style="color:#f97316;">1</strong></span>
  </div>
  <div class="patch-sel-count" id="nh_patchSelCount"></div>
  <div class="patch-btn-row">
    <button class="net-btn sm" onclick="nh_resetPatchCounter()">RESET</button>
    <button class="net-btn sm" style="color:var(--danger);border-color:var(--danger);" onclick="nh_clearFixIDs()">CLEAR FIX IDs</button>
    <button class="net-btn sm" onclick="nh_togglePatchMode()">CANCEL</button>
    <button class="net-btn sm orange" onclick="nh_patchDone()">DONE &mdash; PATCH ALL SELECTED</button>
  </div>
</div>

<div style="overflow-x:auto;">
  <table class="heads-table">
    <thead>
      <tr>
        <th style="width:12px;"></th>
        <th></th>
        <th>Fix#</th>
        <th>Name</th>
        <th>MAC</th>
        <th>IP</th>
        <th>Uni</th>
        <th>Addr</th>
        <th>Role</th>
        <th>Identify</th>
      </tr>
    </thead>
    <tbody id="nh_tbody">
      <tr><td colspan="10" style="color:var(--text-dim);text-align:center;padding:20px 0;">Scanning...</td></tr>
    </tbody>
  </table>
</div>

<script>
(function(){
  var headsData=[],fixturePool=[],artnetPatch=[],selectedMACs=[],selectedOfflineFixIDs=[],identTimers={};
  var patchMode=false, patchNextID=1;
  var FIX_HUES=[200,30,120,270,0,60,160,300,45,330];

  function fixDot(fixID){
    if(!(fixID>0)) return '<td></td>';
    var hue=FIX_HUES[(fixID-1)%FIX_HUES.length];
    return '<td onclick="if(typeof an_selectFix===\'function\')an_selectFix('+fixID+')" style="cursor:pointer;" title="Arm Fix#'+fixID+' in Art-Net grid"><div style="width:8px;height:8px;border-radius:2px;background:hsl('+hue+',68%,52%);"></div></td>';
  }

  nh_load();
  setInterval(nh_load,2000);

  function nh_load(){
    Promise.all([
      fetch('/api/heads').then(function(r){return r.json();}),
      fetch('/api/fixtures').then(function(r){return r.json();}).catch(function(){return[];}),
      fetch('/api/artnet/patch').then(function(r){return r.json();}).catch(function(){return[];})
    ]).then(function(res){headsData=res[0];fixturePool=res[1];artnetPatch=res[2];nh_render();}).catch(function(){});
  }

  function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}

  function nh_render(){
    var tbody=document.getElementById('nh_tbody');
    var active=document.activeElement;if(active&&tbody&&tbody.contains(active))return;
    tbody.innerHTML='';
    if(!headsData||!headsData.length){
      tbody.innerHTML='<tr><td colspan="10" style="color:var(--text-dim);text-align:center;padding:20px 0;">No heads found</td></tr>';
    } else {
      var fixCount={};
      headsData.forEach(function(h){if(h.fixID>0)fixCount[h.fixID]=(fixCount[h.fixID]||0)+1;});
      headsData.sort(function(a,b){
        if(a.role==='LEADER'&&b.role!=='LEADER')return -1;
        if(b.role==='LEADER'&&a.role!=='LEADER')return 1;
        if(a.fixID>0&&b.fixID>0)return a.fixID-b.fixID;
        if(a.fixID>0)return -1;if(b.fixID>0)return 1;
        return a.mac.localeCompare(b.mac);
      });
      headsData.forEach(function(h){
        var sel=selectedMACs.indexOf(h.mac)>=0,dup=h.fixID>0&&fixCount[h.fixID]>1;
        var patch=artnetPatch.find(function(p){return p.fixID===h.fixID;});
        var uniVal=patch?patch.universe:'', addrVal=patch?patch.startAddr:'';
        var tr=document.createElement('tr');
        if(sel)tr.className='selected-head';
        tr.innerHTML=
          fixDot(h.fixID)+
          '<td><input type="checkbox"'+(sel?' checked':'')+' onchange="nh_toggleSel(\''+h.mac+'\',this.checked)"></td>'+
          '<td><input class="fix-id-input'+(dup?' duplicate':'')+'" type="number" min="1" max="999" value="'+(h.fixID>0?h.fixID:'')+'" placeholder="-"'+(dup?' title="WARNING: Duplicate FixID"':'')+' onchange="nh_setFixID(\''+h.mac+'\',this.value)">'+(dup?'<span class="dup-warn">DUP</span>':'')+
          '</td>'+
          '<td><input class="name-input" type="text" value="'+esc(h.name||'')+'" placeholder="Name..." onchange="nh_setName(\''+h.mac+'\',this.value)"></td>'+
          '<td style="color:var(--text-dim);" title="'+h.mac+'">...'+h.mac.slice(-8)+'</td>'+
          '<td style="color:var(--accent);">'+h.ip+'</td>'+
          '<td><input class="patch-num" type="number" min="0" max="32767" value="'+uniVal+'" placeholder="\u2014"'+(h.fixID>0?'':' disabled')+' onchange="nh_setPatchField('+h.fixID+',\'universe\',this.value)"></td>'+
          '<td><input class="patch-num" type="number" min="1" max="512" value="'+addrVal+'" placeholder="\u2014"'+(h.fixID>0?'':' disabled')+' style="color:var(--accent);" onchange="nh_setPatchField('+h.fixID+',\'startAddr\',this.value)"></td>'+
          '<td><span class="role-badge '+(h.role==='LEADER'?'leader':'follower')+'">'+h.role+'</span></td>'+
          '<td><button class="identify-btn" data-mac="'+h.mac+'" onmousedown="nh_identStart(\''+h.mac+'\')" onmouseup="nh_identStop(\''+h.mac+'\')" onmouseleave="nh_identStop(\''+h.mac+'\')" ontouchstart="nh_identStart(\''+h.mac+'\')" ontouchend="nh_identStop(\''+h.mac+'\')">HOLD</button></td>';
        tbody.appendChild(tr);
      });
    }

    var onFix=headsData.map(function(h){return h.fixID;}),onMac=headsData.map(function(h){return h.mac;});
    var offline=fixturePool.filter(function(f){return !(f.mac&&onMac.indexOf(f.mac)>=0)&&!(f.id>0&&onFix.indexOf(f.id)>=0);});
    if(offline.length>0){
      var sep=document.createElement('tr');
      sep.innerHTML='<td colspan="10" style="text-align:center;color:var(--text-dim);font-size:9px;letter-spacing:2px;padding:5px 0;border-top:1px solid var(--border);">-- OFFLINE --</td>';
      tbody.appendChild(sep);
      offline.forEach(function(f){
        var sel=selectedOfflineFixIDs.indexOf(f.id)>=0,tr=document.createElement('tr');
        var opatch=artnetPatch.find(function(p){return p.fixID===f.id;});
        var ouniVal=opatch?opatch.universe:'', oaddrVal=opatch?opatch.startAddr:'';
        tr.style.opacity='0.55';
        tr.innerHTML=
          fixDot(f.id)+
          '<td><input type="checkbox"'+(sel?' checked':'')+' onchange="nh_toggleOffline('+f.id+',this.checked)"></td>'+
          '<td><span style="color:var(--accent2);font-family:var(--mono);font-size:11px;">'+(f.id>0?f.id:'?')+'</span></td>'+
          '<td><input class="name-input" type="text" value="'+esc(f.name||'')+'" placeholder="Name..." onchange="nh_renameFixture('+f.id+',this.value)"></td>'+
          '<td style="color:var(--text-dim);">'+(f.mac?('...'+f.mac.slice(-8)):'--')+'</td>'+
          '<td>--</td>'+
          '<td><input class="patch-num" type="number" min="0" max="32767" value="'+ouniVal+'" placeholder="\u2014"'+(f.id>0?'':' disabled')+' onchange="nh_setPatchField('+f.id+',\'universe\',this.value)"></td>'+
          '<td><input class="patch-num" type="number" min="1" max="512" value="'+oaddrVal+'" placeholder="\u2014"'+(f.id>0?'':' disabled')+' style="color:var(--accent);" onchange="nh_setPatchField('+f.id+',\'startAddr\',this.value)"></td>'+
          '<td><span class="role-badge" style="background:rgba(107,107,138,0.15);color:var(--text-dim)">OFFLINE</span></td>'+
          '<td><button class="identify-btn" onclick="nh_deleteFixture('+f.id+')" style="color:var(--danger);border-color:var(--danger);">X</button></td>';
        tbody.appendChild(tr);
      });
    }

    var addRow=document.createElement('tr');
    addRow.innerHTML=
      '<td></td>'+
      '<td></td>'+
      '<td><input type="number" id="nh_newId" min="1" max="999" placeholder="ID" class="add-input" style="width:38px;color:var(--accent2);text-align:center;"></td>'+
      '<td><input type="text" id="nh_newName" placeholder="Name..." class="add-input" style="width:72px;"></td>'+
      '<td><input type="text" id="nh_newMac" placeholder="MAC (opt)" class="add-input" style="width:80px;font-size:10px;color:var(--text-dim);"></td>'+
      '<td colspan="3"></td>'+
      '<td colspan="2"><button class="identify-btn" onclick="nh_addFixture()" style="color:var(--success);border-color:var(--success);">+ ADD</button></td>';
    tbody.appendChild(addRow);
    nh_updateSelInfo();
    if(patchMode) nh_updatePatchLabel();
  }

  // ── Selection ─────────────────────────────────────────────────────

  window.nh_toggleSel=function(mac,checked){
    if(checked){if(selectedMACs.indexOf(mac)<0)selectedMACs.push(mac);}
    else{selectedMACs=selectedMACs.filter(function(m){return m!==mac;});}
    nh_render();
  };
  window.nh_toggleOffline=function(fixId,checked){
    if(checked){if(selectedOfflineFixIDs.indexOf(fixId)<0)selectedOfflineFixIDs.push(fixId);}
    else{selectedOfflineFixIDs=selectedOfflineFixIDs.filter(function(i){return i!==fixId;});}
    nh_render();
  };
  window.nh_selectAll=function(){
    selectedMACs=headsData.map(function(h){return h.mac;});
    var onFix=headsData.map(function(h){return h.fixID;}),onMac=headsData.map(function(h){return h.mac;});
    selectedOfflineFixIDs=fixturePool.filter(function(f){return !(f.mac&&onMac.indexOf(f.mac)>=0)&&!(f.id>0&&onFix.indexOf(f.id)>=0);}).map(function(f){return f.id;});
    nh_render();
  };
  window.nh_clearSel=function(){selectedMACs=[];selectedOfflineFixIDs=[];nh_render();};

  window.nh_selectNoFix=function(){
    selectedMACs=headsData.filter(function(h){return !(h.fixID>0);}).map(function(h){return h.mac;});
    selectedOfflineFixIDs=[];
    nh_render();
    if(typeof toast==='function') toast(selectedMACs.length+' head(s) without Fix#');
  };

  window.nh_selectNoDmx=function(){
    fetch('/api/artnet/patch').then(function(r){return r.json();}).then(function(patches){
      var patchedIDs=patches.map(function(p){return p.fixID;});
      selectedMACs=headsData.filter(function(h){
        return !(h.fixID>0)||patchedIDs.indexOf(h.fixID)<0;
      }).map(function(h){return h.mac;});
      selectedOfflineFixIDs=[];
      nh_render();
      if(typeof toast==='function') toast(selectedMACs.length+' head(s) without DMX patch');
    });
  };

  function nh_updateSelInfo(){
    var el=document.getElementById('nh_selInfo');
    var labels=selectedMACs.map(function(mac){var h=headsData.find(function(x){return x.mac===mac;});return(h&&h.fixID>0)?'Fix#'+h.fixID:'...'+mac.slice(-5);});
    selectedOfflineFixIDs.forEach(function(id){labels.push('Fix#'+id+'(off)');});
    el.textContent=labels.length?labels.join(', '):'No heads selected';
  }

  // ── Patch mode ────────────────────────────────────────────────────

  window.nh_togglePatchMode=function(){
    patchMode=!patchMode;
    var btn=document.getElementById('nh_patchToggle');
    var bar=document.getElementById('nh_patchBar');
    if(patchMode){
      bar.classList.add('visible');
      patchNextID=parseInt(document.getElementById('nh_patchStart').value)||1;
      nh_updatePatchLabel();
    } else {
      bar.classList.remove('visible');
    }
  };

  window.nh_resetPatchCounter=function(){
    patchNextID=parseInt(document.getElementById('nh_patchStart').value)||1;
    nh_updatePatchLabel();
  };

  function nh_updatePatchLabel(){
    var el=document.getElementById('nh_patchNextLabel');
    if(el) el.textContent=patchNextID;
    var selEl=document.getElementById('nh_patchSelCount');
    if(selEl){
      var n=selectedMACs.length;
      selEl.textContent=n?'('+n+' selected \u2192 Fix#'+patchNextID+'\u2013Fix#'+(patchNextID+n-1)+')':'';
    }
  }

  window.nh_patchDone=function(){
    var sorted=headsData.slice().sort(function(a,b){
      if(a.role==='LEADER'&&b.role!=='LEADER')return -1;
      if(b.role==='LEADER'&&a.role!=='LEADER')return 1;
      if(a.fixID>0&&b.fixID>0)return a.fixID-b.fixID;
      if(a.fixID>0)return -1;if(b.fixID>0)return 1;
      return a.mac.localeCompare(b.mac);
    }).filter(function(h){return selectedMACs.indexOf(h.mac)>=0;});
    if(!sorted.length){nh_togglePatchMode();return;}
    var startId=patchNextID;
    var pending=sorted.length;
    sorted.forEach(function(h,i){
      var id=startId+i;
      fetch('/api/heads/'+h.mac+'/fixid',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({fixID:id})})
        .then(function(){
          h.fixID=id;
          if(--pending===0){
            patchNextID=startId+sorted.length;
            nh_togglePatchMode();
            nh_load();
            if(typeof toast==='function') toast('Patched '+sorted.length+' head(s): Fix#'+startId+'\u2013Fix#'+(startId+sorted.length-1));
          }
        });
    });
  };

  window.nh_clearFixIDs=function(){
    var targets=selectedMACs.slice();
    if(!targets.length){if(typeof toast==='function')toast('No heads selected','err');return;}
    var pending=targets.length;
    targets.forEach(function(mac){
      fetch('/api/heads/'+mac+'/fixid',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({fixID:0})})
        .then(function(){
          headsData.forEach(function(h){if(h.mac===mac)h.fixID=0;});
          if(--pending===0){if(typeof toast==='function')toast('Cleared Fix IDs on '+targets.length+' head(s)');nh_load();}
        });
    });
  };

  // ── Art-Net inline patch editing ──────────────────────────────────

  window.nh_setPatchField=function(fixID,field,val){
    if(!(fixID>0)){if(typeof toast==='function')toast('Set Fix# first','err');return;}
    var existing=artnetPatch.find(function(p){return p.fixID===fixID;});
    var data;
    if(existing){
      data={universe:existing.universe,startAddr:existing.startAddr};
      data[field]=parseInt(val)||(field==='startAddr'?1:0);
      fetch('/api/artnet/patch/'+fixID,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})
        .then(function(r){return r.json();})
        .then(function(d){if(d.status==='ok'){if(typeof toast==='function')toast('Patch updated');nh_load();}});
    } else {
      data={fixID:fixID,universe:0,startAddr:1};
      data[field]=parseInt(val)||(field==='startAddr'?1:0);
      fetch('/api/artnet/patch',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})
        .then(function(r){return r.json();})
        .then(function(d){if(d.status==='ok'){if(typeof toast==='function')toast('Patch created');nh_load();}});
    }
  };

  // ── Fixture management ────────────────────────────────────────────

  window.nh_setFixID=function(mac,val){
    var id=parseInt(val)||0;
    fetch('/api/heads/'+mac+'/fixid',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({fixID:id})}).then(function(){headsData.forEach(function(h){if(h.mac===mac)h.fixID=id;});nh_render();if(typeof toast==='function')toast('Fix#'+id+' saved');});
  };
  window.nh_setName=function(mac,name){
    fetch('/api/heads/'+mac+'/name',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:name})}).then(function(){headsData.forEach(function(h){if(h.mac===mac)h.name=name;});if(typeof toast==='function')toast('Name saved');});
  };
  window.nh_renameFixture=function(id,name){
    fetch('/api/fixtures/'+id,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:name})}).then(function(){fixturePool.forEach(function(f){if(f.id===id)f.name=name;});if(typeof toast==='function')toast('Fixture renamed');});
  };
  window.nh_addFixture=function(){
    var idEl=document.getElementById('nh_newId'),nameEl=document.getElementById('nh_newName'),macEl=document.getElementById('nh_newMac');
    var id=parseInt(idEl.value)||0;
    if(id<=0){if(typeof toast==='function')toast('Enter a valid Fix# ID','err');return;}
    fetch('/api/fixtures',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:id,name:nameEl.value.trim(),mac:macEl.value.trim()||null})}).then(function(r){return r.json();}).then(function(d){
      if(d.status==='ok'){idEl.value='';nameEl.value='';macEl.value='';if(typeof toast==='function')toast('Fixture #'+id+' added');nh_load();}
      else{if(typeof toast==='function')toast(d.message||'Error','err');}
    }).catch(function(){idEl.value='';nameEl.value='';if(typeof toast==='function')toast('Fixture #'+id+' noted');nh_load();});
  };
  window.nh_deleteFixture=function(id){
    fetch('/api/fixtures/'+id,{method:'DELETE'}).then(function(){fixturePool=fixturePool.filter(function(f){return f.id!==id;});selectedOfflineFixIDs=selectedOfflineFixIDs.filter(function(i){return i!==id;});nh_render();if(typeof toast==='function')toast('Fixture removed');});
  };

  // ── Live send / cue ───────────────────────────────────────────────

  window.nh_sendLive=function(){
    if(!selectedMACs.length){if(typeof toast==='function')toast('Select heads first','err');return;}
    var v=(typeof getValues==='function')?getValues():null;
    if(!v){if(typeof toast==='function')toast('Faders not ready','err');return;}
    var cmd='R:'+v.r+',G:'+v.g+',B:'+v.b+',W:'+v.w+',PAN:'+v.pan+',TILT:'+v.tilt;
    fetch('/api/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:cmd,targets:selectedMACs.slice()})});
    if(typeof toast==='function')toast('Sent to '+selectedMACs.length+' head(s)');
  };

  window.nh_addToCue=function(){
    if(!selectedMACs.length&&!selectedOfflineFixIDs.length){if(typeof toast==='function')toast('Select heads first','err');return;}
    var nameEl=document.getElementById('cueName'),name=nameEl?nameEl.value.trim():'';
    if(!name){if(typeof toast==='function')toast('Enter cue name first','err');return;}
    var v=(typeof getValues==='function')?getValues():{r:0,g:0,b:0,w:0,pan:90,tilt:90};
    var fixIds=selectedMACs.map(function(mac){var h=headsData.find(function(x){return x.mac===mac;});return h?h.fixID:0;}).filter(function(id){return id>0;});
    fixIds=fixIds.concat(selectedOfflineFixIDs.filter(function(id){return fixIds.indexOf(id)<0;}));
    if(!fixIds.length)fixIds=[0];
    fetch('/api/cues',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:name,r:v.r,g:v.g,b:v.b,w:v.w,pan:v.pan,tilt:v.tilt,fixTargets:fixIds})}).then(function(){
      if(nameEl)nameEl.value='';
      var tStr=fixIds.map(function(id){return id===0?'ALL':'Fix#'+id;}).join(', ');
      if(typeof toast==='function')toast('Cue saved \u2192 '+tStr);
      if(typeof loadCues==='function')loadCues();
    });
  };

  // ── Identify ──────────────────────────────────────────────────────

  window.nh_identStart=function(mac){if(identTimers[mac])return;nh_identSend(mac,true);identTimers[mac]=setInterval(function(){nh_identSend(mac,true);},500);var btn=document.querySelector('.identify-btn[data-mac="'+mac+'"]');if(btn)btn.classList.add('holding');};
  window.nh_identStop=function(mac){if(!identTimers[mac])return;clearInterval(identTimers[mac]);delete identTimers[mac];nh_identSend(mac,false);var btn=document.querySelector('.identify-btn[data-mac="'+mac+'"]');if(btn)btn.classList.remove('holding');};
  function nh_identSend(mac,on){fetch('/api/heads/'+mac+'/identify',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:on})});}

  // ── Exports ───────────────────────────────────────────────────────

  window.nh_getSelectedMACs=function(){return selectedMACs.slice();};
  window.nh_getSelectedFixIDs=function(){
    var online=selectedMACs.map(function(mac){var h=headsData.find(function(x){return x.mac===mac;});return h?h.fixID:0;}).filter(function(id){return id>0;});
    return online.concat(selectedOfflineFixIDs.filter(function(id){return online.indexOf(id)<0;}));
  };

})();
</script>
)=====";
