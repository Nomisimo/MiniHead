#pragma once

// ── Art-Net Patch Panel HTML Fragment ────────────────────────────
// Served at GET /plugins/artnet/panel.html
// Injected into #artnet-module-container by the main page JS.
// All JS globals are prefixed an_ and wrapped in an IIFE.
// ─────────────────────────────────────────────────────────────────

const char ARTNET_PANEL_HTML[] PROGMEM = R"=====(
<style>
  .an-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:16px;padding-bottom:10px;border-bottom:1px solid var(--border);}
  .an-row{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-bottom:10px;}
  .an-label{font-family:var(--mono);font-size:11px;color:var(--text-dim);white-space:nowrap;}
  .an-num{width:52px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:4px 6px;font-family:var(--mono);font-size:12px;border-radius:3px;text-align:center;-moz-appearance:textfield;}
  .an-num::-webkit-outer-spin-button,.an-num::-webkit-inner-spin-button{-webkit-appearance:none;}
  .an-uni-nav{display:flex;align-items:center;gap:6px;margin-bottom:8px;}
  #an_grid{display:grid;grid-template-columns:repeat(32,1fr);gap:1px;background:var(--border);border:1px solid var(--border);border-radius:4px;overflow:hidden;margin-bottom:12px;cursor:crosshair;}
  .an-cell{aspect-ratio:1;background:var(--surface2);display:flex;align-items:center;justify-content:center;overflow:hidden;transition:filter 0.08s;font-size:9px;color:rgba(0,0,0,0.8);font-family:var(--mono);font-weight:700;line-height:1;user-select:none;}
  .an-cell:hover{filter:brightness(1.5);}
  .an-cell.an-pending{outline:2px solid var(--accent);outline-offset:-2px;}
  .an-patch-table{width:100%;border-collapse:collapse;font-family:var(--mono);font-size:11px;margin-top:4px;}
  .an-patch-table th{text-align:left;color:var(--text-dim);font-weight:normal;padding:4px 6px;border-bottom:1px solid var(--border);letter-spacing:1px;}
  .an-patch-table td{padding:5px 6px;border-bottom:1px solid rgba(42,42,58,0.5);vertical-align:middle;}
  .an-patch-table tr:last-child td{border-bottom:none;}
  .an-sel-row{background:rgba(0,229,255,0.07)!important;}
  .an-del{width:26px;height:26px;border:1px solid var(--border);background:transparent;color:var(--danger);border-radius:3px;cursor:pointer;font-size:12px;display:flex;align-items:center;justify-content:center;}
  .an-empty{font-family:var(--mono);font-size:11px;color:var(--text-dim);text-align:center;padding:18px 0;}
  .net-btn{padding:6px 12px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:11px;cursor:pointer;border-radius:4px;letter-spacing:1px;transition:all 0.15s;text-transform:uppercase;touch-action:manipulation;}
  .net-btn.primary{border-color:var(--accent);color:var(--accent);background:rgba(0,229,255,0.08);}
  .net-btn.sm{padding:3px 8px;font-size:10px;}
  .an-armed-banner{font-family:var(--mono);font-size:10px;color:var(--accent);background:rgba(0,229,255,0.08);border:1px solid var(--accent);border-radius:3px;padding:4px 8px;margin-bottom:8px;display:none;}
  .an-armed-banner.visible{display:block;}
</style>

<!-- Bulk patch row -->
<div class="an-row">
  <span class="an-label">Bulk:</span>
  <span class="an-label">Uni</span>
  <input type="number" id="an_bulkUni" class="an-num" value="0" min="0" max="32767">
  <span class="an-label">.</span>
  <span class="an-label">Addr</span>
  <input type="number" id="an_bulkAddr" class="an-num" value="1" min="1" max="512">
  <span class="an-label">&times;</span>
  <input type="number" id="an_bulkCount" class="an-num" value="1" min="1" max="32" style="width:40px;">
  <span class="an-label">lamps&nbsp;&nbsp;First&nbsp;Fix#</span>
  <input type="number" id="an_bulkFix" class="an-num" value="1" min="1" max="999" style="width:48px;">
  <button class="net-btn primary" onclick="an_bulk()">PATCH</button>
</div>

<!-- Action row: patch selected + clear -->
<div class="an-row" style="margin-bottom:12px;">
  <button class="net-btn primary" onclick="an_patchSelected()">PATCH SELECTED</button>
  <span style="flex:1;"></span>
  <button class="net-btn sm" onclick="an_clearUniverse()">CLEAR UNI</button>
  <button class="net-btn sm" style="color:var(--danger);border-color:var(--danger);" onclick="an_clearAll()">CLEAR ALL</button>
</div>

<!-- Armed fixture banner -->
<div class="an-armed-banner" id="an_armedBanner">Click a grid cell to place armed fixture</div>

<!-- Universe grid navigation -->
<div class="an-uni-nav">
  <span class="an-label">Universe</span>
  <input type="number" id="an_uniInput" class="an-num" value="0" min="0" max="32767" onchange="an_gotoUniverse(parseInt(this.value)||0)">
  <button class="net-btn sm" onclick="an_prevUniverse()">&#9664;</button>
  <button class="net-btn sm" onclick="an_nextUniverse()">&#9654;</button>
  <span class="an-label" id="an_uniInfo" style="margin-left:auto;"></span>
</div>

<!-- 512-slot grid -->
<div id="an_grid"></div>

<!-- Patch table -->
<div id="an_patchTable"></div>

<!-- Conflict detection modal -->
<div id="an_conflictModal" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,0.82);z-index:800;align-items:center;justify-content:center;">
  <div style="background:var(--surface);border:1px solid var(--border);border-radius:6px;padding:20px;max-width:340px;width:90%;max-height:80vh;overflow-y:auto;">
    <div style="font-family:var(--mono);font-size:11px;color:var(--danger);letter-spacing:3px;text-transform:uppercase;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid var(--border);">// Conflict Detected</div>
    <div id="an_conflictMsg" style="font-family:var(--mono);font-size:11px;color:var(--text);margin-bottom:14px;line-height:1.7;white-space:pre-line;"></div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;">
      <button class="net-btn" style="color:var(--danger);border-color:var(--danger);" onclick="an_resolveConflict('overwrite')">OVERWRITE</button>
      <button class="net-btn primary" onclick="an_resolveConflict('merge')">MERGE</button>
      <button class="net-btn" onclick="an_resolveConflict('cancel')">CANCEL</button>
    </div>
  </div>
</div>

<script>
(function(){
  var DMX_FP = 7;
  var an_patches = [], an_fixtures = [];
  var an_curUni = 0;
  var an_selectedFixID = null;
  var FIX_HUES = [200,30,120,270,0,60,160,300,45,330];
  var _conflictOverwriteCb = null, _conflictMergeCb = null;
  var _pendingPatches = null;

  function an_fixColor(fixID){ return 'hsl('+FIX_HUES[(fixID-1)%FIX_HUES.length]+',68%,52%)'; }
  function an_fixColorDim(fixID){ return 'hsla('+FIX_HUES[(fixID-1)%FIX_HUES.length]+',68%,38%,0.35)'; }

  function an_load(){
    Promise.all([
      fetch('/api/artnet/patch').then(function(r){return r.json();}),
      fetch('/api/fixtures').then(function(r){return r.json();}).catch(function(){return[];})
    ]).then(function(res){
      an_patches=res[0]; an_fixtures=res[1];
      an_renderGrid(); an_renderTable();
    }).catch(function(){});
  }

  function an_gotoUniverse(uni){
    an_curUni = Math.max(0, Math.min(32767, uni));
    document.getElementById('an_uniInput').value = an_curUni;
    an_renderGrid(); an_renderTable();
  }
  function an_prevUniverse(){ an_gotoUniverse(an_curUni - 1); }
  function an_nextUniverse(){ an_gotoUniverse(an_curUni + 1); }

  function an_renderGrid(){
    var grid = document.getElementById('an_grid');
    grid.innerHTML = '';
    var slotMap = {};
    an_patches.forEach(function(p){
      if(p.universe !== an_curUni) return;
      for(var ch=0; ch<DMX_FP; ch++) slotMap[p.startAddr+ch] = p;
    });
    var count = an_patches.filter(function(p){return p.universe===an_curUni;}).length;
    document.getElementById('an_uniInfo').textContent = count ? count+' fixture(s)' : '';
    for(var i=1; i<=512; i++){
      var cell = document.createElement('div');
      cell.className = 'an-cell';
      cell.dataset.addr = i;
      var p = slotMap[i];
      if(p){
        if(i === p.startAddr){
          cell.style.background = an_fixColor(p.fixID);
          cell.innerHTML = '<div style="display:flex;flex-direction:column;align-items:center;line-height:1.2;">'
            + '<span style="font-size:11px;font-weight:700;color:rgba(0,0,0,0.85);">'+p.fixID+'</span>'
            + '<span style="font-size:7px;font-weight:400;color:rgba(0,0,0,0.5);">'+i+'</span>'
            + '</div>';
          cell.title = 'Fix#'+p.fixID+' \u2014 Addr '+i;
        } else {
          var relCh = i - p.startAddr + 1;
          cell.style.background = an_fixColorDim(p.fixID);
          cell.innerHTML = '<span style="font-size:11px;color:rgba(255,255,255,0.7);">'+relCh+'</span>';
          cell.title = 'Fix#'+p.fixID+' \u2014 Addr '+i+' (ch '+relCh+')';
        }
      } else {
        cell.innerHTML = '<span style="font-size:9px;opacity:0.25;color:#fff;">'+i+'</span>';
        cell.title = 'Addr '+i+' (free)';
      }
      if(an_selectedFixID !== null && p && p.fixID === an_selectedFixID) cell.classList.add('an-pending');
      cell.addEventListener('click', function(){
        an_cellClick(parseInt(this.dataset.addr));
      });
      grid.appendChild(cell);
    }
  }

  function an_cellClick(addr){
    if(an_selectedFixID === null){
      if(typeof toast==='function') toast('Select a fixture row first','err');
      return;
    }
    if(addr + DMX_FP - 1 > 512){
      if(typeof toast==='function') toast('Not enough room \u2014 use next universe','err');
      return;
    }
    var newPatch = [{fixID:an_selectedFixID, universe:an_curUni, startAddr:addr}];
    var r = an_getBulkResult(newPatch);
    if(!r.conflicts.length){
      an_doPatch(an_selectedFixID, an_curUni, addr, function(){ an_selectedFixID=null; an_updateArmBanner(); an_load(); });
    } else {
      var msg = 'Fix#'+r.conflicts.join(', Fix#')+' overlaps here.\nOVERWRITE to replace, MERGE to skip.';
      an_showConflict(msg,
        function(){
          an_deleteConflicts(r.conflicts, function(){
            an_doPatch(an_selectedFixID, an_curUni, addr, function(){ an_selectedFixID=null; an_updateArmBanner(); an_load(); });
          });
        },
        function(){ if(typeof toast==='function') toast('Cancelled — conflict not resolved'); }
      );
    }
  }

  function an_renderTable(){
    var el = document.getElementById('an_patchTable');
    if(!an_patches.length){
      el.innerHTML='<div class="an-empty">No fixtures patched yet</div>';
      return;
    }
    var fix_map={};
    an_fixtures.forEach(function(f){fix_map[f.id]=f.name||('Fix#'+f.id);});
    var html='<table class="an-patch-table"><thead><tr>'
      +'<th></th><th>Fix#</th><th>Name</th><th>Universe</th><th>Addr</th><th>Ch</th><th></th>'
      +'</tr></thead><tbody>';
    an_patches.forEach(function(p){
      var sel = an_selectedFixID===p.fixID;
      var name = fix_map[p.fixID]||('Fix#'+p.fixID);
      html+='<tr class="'+(sel?'an-sel-row':'')+'" data-fid="'+p.fixID+'">'
        +'<td onclick="an_selectFix('+p.fixID+')" style="cursor:pointer;"><div style="width:10px;height:10px;border-radius:2px;background:'+an_fixColor(p.fixID)+';"></div></td>'
        +'<td onclick="an_selectFix('+p.fixID+')" style="cursor:pointer;color:var(--accent2);">'+p.fixID+'</td>'
        +'<td onclick="an_selectFix('+p.fixID+')" style="cursor:pointer;">'+name+'</td>'
        +'<td><input type="number" class="an-num" style="width:44px;" value="'+p.universe+'" min="0" max="32767"'
          +' onclick="event.stopPropagation()" onchange="an_updatePatch('+p.fixID+',+this.value,null)"></td>'
        +'<td><input type="number" class="an-num" style="width:44px;color:var(--accent);" value="'+p.startAddr+'" min="1" max="512"'
          +' onclick="event.stopPropagation()" onchange="an_updatePatch('+p.fixID+',null,+this.value)"></td>'
        +'<td style="color:var(--text-dim);font-size:10px;">'+p.startAddr+'\u2013'+(p.startAddr+DMX_FP-1)+'</td>'
        +'<td><button class="an-del" onclick="an_del(event,'+p.fixID+')">&#10005;</button></td>'
        +'</tr>';
    });
    html+='</tbody></table>';
    el.innerHTML=html;
  }

  // ── Conflict detection ────────────────────────────────────────────

  function an_getBulkResult(newPatches){
    var conflicts = [];
    var safe = [];
    newPatches.forEach(function(np){
      var newEnd = np.startAddr + DMX_FP - 1;
      var hasConflict = false;
      an_patches.forEach(function(p){
        if(p.fixID === np.fixID) return;
        if(p.universe !== np.universe) return;
        var pEnd = p.startAddr + DMX_FP - 1;
        if(np.startAddr <= pEnd && newEnd >= p.startAddr){
          hasConflict = true;
          if(conflicts.indexOf(p.fixID) < 0) conflicts.push(p.fixID);
        }
      });
      if(!hasConflict) safe.push(np);
    });
    return { conflicts: conflicts, safe: safe };
  }

  function an_buildMsg(total, safe, outOfRange, conflictIDs){
    var lines = [];
    if(outOfRange > 0) lines.push(outOfRange+' fixture(s) exceed Uni '+an_curUni+' address space (max 512) — skipped.');
    if(conflictIDs.length) lines.push('Conflicts with existing: Fix#'+conflictIDs.join(', Fix#'));
    lines.push('');
    lines.push('OVERWRITE — remove conflicting patches, place all '+total);
    lines.push('MERGE     — skip conflicts, place '+safe+' non-conflicting');
    lines.push('CANCEL    — do nothing');
    return lines.join('\n');
  }

  function an_showConflict(msg, overwriteCb, mergeCb){
    _conflictOverwriteCb = overwriteCb;
    _conflictMergeCb = mergeCb;
    document.getElementById('an_conflictMsg').textContent = msg;
    document.getElementById('an_conflictModal').style.display = 'flex';
  }

  window.an_resolveConflict = function(choice){
    document.getElementById('an_conflictModal').style.display = 'none';
    if(choice==='overwrite' && _conflictOverwriteCb) _conflictOverwriteCb();
    else if(choice==='merge' && _conflictMergeCb) _conflictMergeCb();
    _conflictOverwriteCb = null; _conflictMergeCb = null;
  };

  function an_deleteConflicts(conflictIDs, cb){
    var pending = conflictIDs.length;
    if(!pending){ cb(); return; }
    conflictIDs.forEach(function(fid){
      fetch('/api/artnet/patch/'+fid, {method:'DELETE'})
        .then(function(){ if(--pending===0) cb(); })
        .catch(function(){ if(--pending===0) cb(); });
    });
  }

  function an_doPatch(fixID, uni, addr, cb){
    fetch('/api/artnet/patch', {method:'POST', headers:{'Content-Type':'application/json'},
      body:JSON.stringify({fixID:fixID, universe:uni, startAddr:addr})
    }).then(function(r){return r.json();}).then(function(d){
      if(d.status==='ok'){ if(typeof toast==='function') toast('Fix#'+fixID+' patched \u2192 Uni '+uni+' Addr '+addr); if(cb)cb(); }
      else if(typeof toast==='function') toast(d.message||'Error','err');
    });
  }

  function an_executeBatch(patches, msg){
    var pending = patches.length;
    if(!pending){ if(typeof toast==='function') toast('Nothing to patch'); return; }
    patches.forEach(function(p){
      fetch('/api/artnet/patch', {method:'POST', headers:{'Content-Type':'application/json'},
        body:JSON.stringify({fixID:p.fixID, universe:p.universe, startAddr:p.startAddr})
      }).then(function(){
        if(--pending===0){
          if(typeof toast==='function') toast(msg);
          an_gotoUniverse(patches[0].universe);
          an_load();
        }
      }).catch(function(){ if(--pending===0){ an_load(); } });
    });
  }

  // ── Actions ───────────────────────────────────────────────────────

  window.an_patchSelected = function(){
    var ids = (typeof nh_getSelectedFixIDs==='function') ? nh_getSelectedFixIDs() : [];
    if(!ids.length){ if(typeof toast==='function') toast('No heads selected in Network panel','err'); return; }
    var uni  = parseInt(document.getElementById('an_bulkUni').value)  || 0;
    var addr = parseInt(document.getElementById('an_bulkAddr').value) || 1;
    var inRange=[], outOfRange=0;
    ids.forEach(function(fixID, i){
      var a = addr + i * DMX_FP;
      if(a + DMX_FP - 1 > 512){ outOfRange++; }
      else { inRange.push({fixID:fixID, universe:uni, startAddr:a}); }
    });
    if(!inRange.length){ if(typeof toast==='function') toast('No room in universe '+uni,'err'); return; }
    var r = an_getBulkResult(inRange);
    if(!r.conflicts.length && !outOfRange){
      an_executeBatch(inRange, 'Patched '+inRange.length+' fixture(s)');
      return;
    }
    var msg = an_buildMsg(inRange.length, r.safe.length, outOfRange, r.conflicts);
    an_showConflict(msg,
      function(){ an_deleteConflicts(r.conflicts, function(){ an_executeBatch(inRange, 'Patched '+inRange.length+' fixture(s)'); }); },
      function(){ an_executeBatch(r.safe, 'Merged: patched '+r.safe.length+' fixture(s)'); }
    );
  };

  window.an_clearUniverse = function(){
    fetch('/api/artnet/patch/universe/'+an_curUni, {method:'DELETE'})
      .then(function(r){return r.json();})
      .then(function(d){ if(typeof toast==='function') toast('Universe '+an_curUni+' cleared ('+(d.removed||0)+' patch(es))'); an_load(); })
      .catch(function(){ if(typeof toast==='function') toast('Clear failed','err'); });
  };

  window.an_clearAll = function(){
    if(!an_patches.length){ if(typeof toast==='function') toast('Nothing to clear'); return; }
    fetch('/api/artnet/patch', {method:'DELETE'})
      .then(function(r){return r.json();})
      .then(function(d){ if(typeof toast==='function') toast('All patches cleared ('+(d.removed||0)+')'); an_load(); })
      .catch(function(){ if(typeof toast==='function') toast('Clear failed','err'); });
  };

  // ── Existing actions ──────────────────────────────────────────────

  function an_updateArmBanner(){
    var banner = document.getElementById('an_armedBanner');
    if(!banner) return;
    if(an_selectedFixID !== null){
      banner.textContent = 'Fix#'+an_selectedFixID+' armed \u2014 click a grid cell to place';
      banner.classList.add('visible');
    } else {
      banner.classList.remove('visible');
    }
  }

  window.an_selectFix=function(fixID){
    an_selectedFixID = (an_selectedFixID===fixID) ? null : fixID;
    var p = an_patches.find(function(x){return x.fixID===fixID;});
    if(p && an_selectedFixID!==null) an_gotoUniverse(p.universe);
    else { an_renderGrid(); an_renderTable(); }
    an_updateArmBanner();
    if(typeof toast==='function' && an_selectedFixID!==null) toast('Fix#'+fixID+' armed \u2014 click a grid cell to place');
  };

  window.an_updatePatch=function(fixID, uni, addr){
    var p=an_patches.find(function(x){return x.fixID===fixID;});
    if(!p) return;
    var data={universe:uni!==null?uni:p.universe, startAddr:addr!==null?addr:p.startAddr};
    fetch('/api/artnet/patch/'+fixID,{method:'PUT',headers:{'Content-Type':'application/json'},
      body:JSON.stringify(data)
    }).then(function(r){return r.json();}).then(function(d){
      if(d.status==='ok'){
        if(uni!==null){ p.universe=uni; if(an_selectedFixID===fixID) an_gotoUniverse(uni); }
        if(addr!==null) p.startAddr=addr;
        an_renderGrid(); an_renderTable();
        if(typeof toast==='function') toast('Patch updated');
      }
    });
  };

  window.an_del=function(e,fixID){
    e.stopPropagation();
    fetch('/api/artnet/patch/'+fixID,{method:'DELETE'}).then(function(){
      if(typeof toast==='function') toast('Patch removed');
      if(an_selectedFixID===fixID){ an_selectedFixID=null; an_updateArmBanner(); }
      an_load();
    });
  };

  window.an_bulk=function(){
    var uni   = parseInt(document.getElementById('an_bulkUni').value)   || 0;
    var addr  = parseInt(document.getElementById('an_bulkAddr').value)  || 1;
    var count = parseInt(document.getElementById('an_bulkCount').value) || 1;
    var fix   = parseInt(document.getElementById('an_bulkFix').value)   || 1;
    fetch('/api/artnet/patch/bulk',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({universe:uni,startAddr:addr,count:count,firstFixID:fix})
    }).then(function(r){return r.json();}).then(function(d){
      if(d.status==='ok'){
        if(typeof toast==='function') toast('Patched '+d.count+' fixture(s)');
        an_gotoUniverse(uni);
        an_load();
      } else if(typeof toast==='function') toast(d.message||'Error','err');
    });
  };

  window.an_prevUniverse = an_prevUniverse;
  window.an_nextUniverse = an_nextUniverse;
  window.an_gotoUniverse = an_gotoUniverse;

  an_load();
  setInterval(an_load, 5000);
})();
</script>
)=====";
