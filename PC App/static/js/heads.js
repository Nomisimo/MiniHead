/**
 * heads.js — network heads table, patch mode, fixture management.
 * Exports: nh_load(), nh_getSelectedMACs(), nh_getSelectedFixIDs()
 */
'use strict';

let headsData             = [];
let fixturePool           = [];
let selectedMACs          = [];
let selectedOfflineFixIDs = [];
let identTimers           = {};
let patchMode             = false;
let patchNextID           = 1;
let anPatches             = [];
let patchAcks             = {};
let nameAcks              = {};
let nhEditMode            = false;

window.nh_getSelectedMACs   = () => selectedMACs.slice();
window.nh_getSelectedFixIDs = function () {
  const online = selectedMACs.map(mac => (headsData.find(h => h.mac === mac) || {}).fixID).filter(id => id > 0);
  return [...new Set([...online, ...selectedOfflineFixIDs.filter(id => !online.includes(id))])];
};

async function nh_load() {
  if (nhEditMode) return;
  const [heads, fixtures, patches] = await Promise.all([
    api.heads().catch(() => []),
    api.fixtures().catch(() => []),
    api.artnetPatches().catch(() => []),
  ]);
  headsData   = heads;
  fixturePool = fixtures;
  anPatches   = patches;
  // Clean stale acks
  const now = Date.now() / 1000;
  Object.keys(patchAcks).forEach(k => { if (patchAcks[k].status === 'ok' && now - (patchAcks[k].ts || 0) > 20) delete patchAcks[k]; });
  nh_render();
}

window.nh_startEdit = function () {
  nhEditMode = true;
  document.getElementById('nh_editBtn').style.display = 'none';
  document.getElementById('nh_saveBtn').style.display = '';
  document.getElementById('nh_tableWrap').classList.remove('nh-locked');
  toast('Edit mode — refresh paused');
};
window.nh_stopEdit = function () {
  nhEditMode = false;
  document.getElementById('nh_editBtn').style.display = '';
  document.getElementById('nh_saveBtn').style.display = 'none';
  document.getElementById('nh_tableWrap').classList.add('nh-locked');
  nh_load();
  toast('Saved — refresh resumed');
};

function esc(s) { return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'); }

function nh_ackIcon(ack) {
  if (!ack) return '';
  const age = Date.now() / 1000 - (ack.ts || 0);
  if (ack.status === 'pending')               return '<span style="color:var(--accent2);font-size:12px;margin-left:3px;" title="Waiting…">&#8635;</span>';
  if (ack.status === 'ok'     && age < 20)    return '<span style="color:#008000;font-size:12px;margin-left:3px;" title="Confirmed">&#10003;</span>';
  if (ack.status === 'timeout')               return '<span style="color:#800000;font-size:12px;margin-left:3px;" title="No response">&#10007;</span>';
  return '';
}

function nh_render() {
  const tbody = document.getElementById('nh_tbody'); if (!tbody) return;
  tbody.innerHTML = '';

  if (!headsData.length) {
    tbody.innerHTML = '<tr><td colspan="11" style="color:var(--text-dim);text-align:center;padding:20px 0;">No heads found</td></tr>';
  } else {
    const fixCount = {};
    headsData.forEach(h => { if (h.fixID > 0) fixCount[h.fixID] = (fixCount[h.fixID] || 0) + 1; });
    headsData.sort((a, b) => {
      if (a.role === 'LEADER' && b.role !== 'LEADER') return -1;
      if (b.role === 'LEADER' && a.role !== 'LEADER') return 1;
      if (a.fixID > 0 && b.fixID > 0) return a.fixID - b.fixID;
      if (a.fixID > 0) return -1; if (b.fixID > 0) return 1;
      return a.mac.localeCompare(b.mac);
    });

    headsData.forEach(h => {
      const sel       = selectedMACs.includes(h.mac);
      const dup       = h.fixID > 0 && fixCount[h.fixID] > 1;
      const modeClass = h.mode === 'ARTNET' ? 'artnet' : (h.mode === 'PC' ? 'pc' : 'udp');
      const anPatch   = h.fixID > 0 ? anPatches.find(p => p.fixID === h.fixID) : null;
      const patchIcon = nh_ackIcon(patchAcks[h.fixID]);
      const nameIcon  = nh_ackIcon(nameAcks[h.mac]);
      const isArtnet  = h.mode === 'ARTNET';

      let patchCells;
      if (isArtnet && h.fixID > 0 && anPatch) {
        patchCells =
          `<td><input class="patch-num-input" type="number" value="${anPatch.universe}" min="0" max="32767" onchange="nh_patchUpdate(${h.fixID},+this.value,null)"></td>` +
          `<td><input class="patch-num-input" type="number" value="${anPatch.startAddr}" min="1" max="512" onchange="nh_patchUpdate(${h.fixID},null,+this.value)"></td>` +
          `<td style="white-space:nowrap;"><button class="icon-btn del" onclick="nh_patchDel(${h.fixID})">&#10005;</button>${patchIcon}</td>`;
      } else if (isArtnet && h.fixID > 0) {
        patchCells =
          `<td style="color:var(--text-dim);font-size:10px;text-align:center;">—</td><td style="color:var(--text-dim);font-size:10px;text-align:center;">—</td>` +
          `<td><button class="net-btn" style="padding:2px 6px;font-size:10px;" onclick="nh_patchCreate(${h.fixID})">+DMX</button></td>`;
      } else {
        patchCells = `<td style="color:var(--text-dim);text-align:center;">—</td><td style="color:var(--text-dim);text-align:center;">—</td><td></td>`;
      }

      const modeBadge = h.mode ? `<span class="mode-badge ${modeClass}">${h.mode}</span>` : '';
      const tr = document.createElement('tr');
      tr.dataset.mac = h.mac;
      if (sel) tr.className = 'selected-head';
      tr.innerHTML =
        `<td><input type="checkbox"${sel ? ' checked' : ''} onchange="nh_toggleSel('${h.mac}',this.checked)"></td>` +
        `<td><input class="fix-id-input${dup ? ' duplicate' : ''}" type="number" min="1" max="999" value="${h.fixID > 0 ? h.fixID : ''}" placeholder="-"${dup ? ' title="Duplicate FixID"' : ''} onchange="nh_setFixID('${h.mac}',this.value)">${dup ? '<span class="dup-warn">DUP</span>' : ''}</td>` +
        `<td style="white-space:nowrap;"><input class="name-input" type="text" value="${esc(h.name||'')}" placeholder="Name..." onchange="nh_setName('${h.mac}',this.value)">${nameIcon}</td>` +
        `<td style="color:var(--text-dim);" title="${h.mac}">&#8230;${h.mac.slice(-8)}</td>` +
        `<td>${h.ip}</td>` +
        `<td><span class="role-badge ${h.role === 'LEADER' ? 'leader' : 'follower'}">${h.role}</span>${modeBadge}</td>` +
        patchCells +
        `<td class="identify-col"><button class="identify-btn" data-mac="${h.mac}" onmousedown="nh_identStart('${h.mac}')" onmouseup="nh_identStop('${h.mac}')" onmouseleave="nh_identStop('${h.mac}')" ontouchstart="nh_identStart('${h.mac}')" ontouchend="nh_identStop('${h.mac}')">HOLD</button></td>` +
        `<td><button class="patch-assign-btn" onclick="nh_patchAssign('${h.mac}')">&#8594; Fix#${patchNextID}</button></td>`;
      tbody.appendChild(tr);
    });
  }

  // Offline fixtures
  const onlineMACs   = headsData.map(h => h.mac);
  const onlineFixIDs = headsData.map(h => h.fixID);
  const offline      = fixturePool.filter(f => !(f.mac && onlineMACs.includes(f.mac)) && !(f.id > 0 && onlineFixIDs.includes(f.id)));
  if (offline.length) {
    const sep = document.createElement('tr');
    sep.innerHTML = `<td colspan="11" style="text-align:center;color:var(--text-dim);font-size:9px;padding:5px 0;border-top:1px solid var(--border);">── OFFLINE ──</td>`;
    tbody.appendChild(sep);
    offline.forEach(f => {
      const sel      = selectedOfflineFixIDs.includes(f.id);
      const offPatch = f.id > 0 ? anPatches.find(p => p.fixID === f.id) : null;
      const offIcon  = nh_ackIcon(patchAcks[f.id]);
      const offCells = offPatch
        ? `<td><input class="patch-num-input" type="number" value="${offPatch.universe}" min="0" max="32767" onchange="nh_patchUpdate(${f.id},+this.value,null)"></td><td><input class="patch-num-input" type="number" value="${offPatch.startAddr}" min="1" max="512" onchange="nh_patchUpdate(${f.id},null,+this.value)"></td><td><button class="icon-btn del" onclick="nh_patchDel(${f.id})">&#10005;</button>${offIcon}</td>`
        : `<td style="text-align:center;color:var(--text-dim);">—</td><td style="text-align:center;color:var(--text-dim);">—</td><td></td>`;
      const tr = document.createElement('tr'); tr.style.opacity = '0.6';
      tr.innerHTML =
        `<td><input type="checkbox"${sel ? ' checked' : ''} onchange="nh_toggleOffline(${f.id},this.checked)"></td>` +
        `<td><span style="font-family:var(--mono);font-size:11px;">${f.id > 0 ? f.id : '—'}</span></td>` +
        `<td><input class="name-input" type="text" value="${esc(f.name||'')}" placeholder="Name..." onchange="nh_renameFixture(${f.id},this.value)"></td>` +
        `<td style="color:var(--text-dim);">${f.mac ? '&#8230;' + f.mac.slice(-8) : '—'}</td>` +
        `<td>—</td><td><span class="role-badge">OFFLINE</span></td>` +
        offCells +
        `<td class="identify-col"><button class="identify-btn" onclick="nh_deleteFixture(${f.id})" style="color:var(--danger);border-color:var(--danger);">&#10005;</button></td><td></td>`;
      tbody.appendChild(tr);
    });
  }

  // Add fixture row
  const addRow = document.createElement('tr');
  addRow.innerHTML =
    `<td></td>` +
    `<td><input type="number" id="nh_newId" min="1" max="999" placeholder="ID" class="add-input" style="width:38px;text-align:center;"></td>` +
    `<td><input type="text" id="nh_newName" placeholder="Name..." class="add-input" style="width:78px;"></td>` +
    `<td><input type="text" id="nh_newMac" placeholder="MAC (opt)" class="add-input" style="width:88px;font-size:10px;color:var(--text-dim);"></td>` +
    `<td colspan="5"></td>` +
    `<td class="identify-col"><button class="identify-btn" onclick="nh_addFixture()" style="color:var(--success);border-color:var(--success);">+</button></td><td></td>`;
  tbody.appendChild(addRow);

  if (patchMode) tbody.querySelectorAll('.patch-assign-btn').forEach(btn => { btn.textContent = '→ Fix#' + patchNextID; });
  nh_updateSelInfo(); nh_updatePatchLabel();

  const hasArtnet = headsData.some(h => h.mode === 'ARTNET');
  const pgPanel   = document.getElementById('pg_panel');
  const noDmxBtn  = document.getElementById('nh_noDmxBtn');
  const wasHidden = pgPanel?.style.display === 'none';
  if (pgPanel)  pgPanel.style.display  = hasArtnet ? '' : 'none';
  if (noDmxBtn) noDmxBtn.style.display = hasArtnet ? '' : 'none';
  if (hasArtnet && wasHidden && typeof pg_reload === 'function') pg_reload();
}

// ── Selection ─────────────────────────────────────────────────────────────────

window.nh_toggleSel     = (mac, checked) => { if (checked) { if (!selectedMACs.includes(mac)) selectedMACs.push(mac); } else selectedMACs = selectedMACs.filter(m => m !== mac); nh_render(); };
window.nh_toggleOffline = (id, checked)  => { if (checked) { if (!selectedOfflineFixIDs.includes(id)) selectedOfflineFixIDs.push(id); } else selectedOfflineFixIDs = selectedOfflineFixIDs.filter(i => i !== id); nh_render(); };
window.nh_selectAll     = () => { selectedMACs = headsData.map(h => h.mac); selectedOfflineFixIDs = fixturePool.filter(f => !(f.mac && headsData.some(h => h.mac === f.mac)) && !headsData.some(h => h.fixID === f.id)).map(f => f.id); nh_render(); };
window.nh_clearSel      = () => { selectedMACs = []; selectedOfflineFixIDs = []; nh_render(); };
window.nh_selectNoFix   = () => { selectedMACs = headsData.filter(h => !(h.fixID > 0)).map(h => h.mac); selectedOfflineFixIDs = []; nh_render(); toast(selectedMACs.length + ' head(s) without Fix#'); };
window.nh_selectNoDmx   = async () => { const ps = await api.artnetPatches().catch(() => []); const ids = ps.map(p => p.fixID); selectedMACs = headsData.filter(h => !ids.includes(h.fixID)).map(h => h.mac); selectedOfflineFixIDs = []; nh_render(); toast(selectedMACs.length + ' head(s) without DMX patch'); };
window.nh_clearFixIDs   = () => { if (!selectedMACs.length) { toast('No heads selected', 'err'); return; } selectedMACs.forEach(mac => api.setFixID(mac, 0).then(() => { const h = headsData.find(x => x.mac === mac); if (h) h.fixID = 0; })); toast('Cleared Fix IDs'); setTimeout(nh_load, 500); };

function nh_updateSelInfo() {
  const labels = [
    ...selectedMACs.map(mac => { const h = headsData.find(x => x.mac === mac); return h?.fixID > 0 ? 'Fix#' + h.fixID : '…' + mac.slice(-5); }),
    ...selectedOfflineFixIDs.map(id => 'Fix#' + id + '(off)'),
  ];
  document.getElementById('nh_selInfo').textContent = labels.length ? labels.join(', ') : 'No heads selected';
}

// ── Fix ID / Name ─────────────────────────────────────────────────────────────

window.nh_setFixID = async (mac, val) => {
  const id = parseInt(val) || 0;
  await api.setFixID(mac, id).catch(() => {});
  const h = headsData.find(x => x.mac === mac); if (h) h.fixID = id;
  nh_render(); toast('Fix#' + id + ' saved');
};

window.nh_setName = async (mac, name) => {
  nameAcks[mac] = { status: 'pending', ts: Date.now() / 1000 }; nh_render();
  await api.setName(mac, name).catch(() => {});
  const h = headsData.find(x => x.mac === mac); if (h) h.name = name;
  toast('Name sent');
};

// ── Fixture CRUD ──────────────────────────────────────────────────────────────

window.nh_renameFixture = async (id, name) => { await api.updateFixture(id, { name }).catch(() => {}); const f = fixturePool.find(x => x.id === id); if (f) f.name = name; toast('Fixture renamed'); };
window.nh_addFixture    = async () => {
  const id  = parseInt(document.getElementById('nh_newId').value) || 0;
  const name = document.getElementById('nh_newName').value.trim();
  const mac  = document.getElementById('nh_newMac').value.trim() || null;
  if (!id) { toast('Enter a valid Fix#', 'err'); return; }
  await api.createFixture(id, name, mac).catch(() => {});
  document.getElementById('nh_newId').value = document.getElementById('nh_newName').value = document.getElementById('nh_newMac').value = '';
  toast('Fixture #' + id + ' added'); nh_load();
};
window.nh_deleteFixture = async id => { await api.deleteFixture(id).catch(() => {}); fixturePool = fixturePool.filter(f => f.id !== id); selectedOfflineFixIDs = selectedOfflineFixIDs.filter(i => i !== id); nh_render(); toast('Fixture removed'); };

// ── Send live / Add to cue ────────────────────────────────────────────────────

window.nh_sendLive = () => {
  if (!selectedMACs.length) { toast('Select heads first', 'err'); return; }
  const v   = getValues();
  const cmd = `R:${v.r},G:${v.g},B:${v.b},W:${v.w},PAN:${v.pan},TILT:${v.tilt}`;
  api.send(cmd, selectedMACs.slice()).catch(() => {});
  toast('Sent to ' + selectedMACs.length + ' head(s)');
};

window.nh_addToCue = async () => {
  if (!selectedMACs.length && !selectedOfflineFixIDs.length) { toast('Select heads first', 'err'); return; }
  const nameEl = document.getElementById('cueName');
  const name   = nameEl ? nameEl.value.trim() : '';
  if (!name) { toast('Enter cue name first', 'err'); return; }
  const v      = getValues();
  const fixIds = [...new Set([...selectedMACs.map(mac => (headsData.find(h => h.mac === mac) || {}).fixID).filter(id => id > 0), ...selectedOfflineFixIDs])];
  await api.createCue({ name, r: v.r, g: v.g, b: v.b, w: v.w, pan: v.pan, tilt: v.tilt, fixTargets: fixIds.length ? fixIds : [0] }).catch(() => {});
  if (nameEl) nameEl.value = '';
  toast('Cue saved → ' + (fixIds.length ? fixIds.map(id => id === 0 ? 'ALL' : 'Fix#' + id).join(', ') : 'ALL'));
  if (typeof loadCues === 'function') loadCues();
};

// ── Identify ──────────────────────────────────────────────────────────────────

window.nh_identStart = mac => {
  if (identTimers[mac]) return;
  api.identify(mac, true).catch(() => {});
  identTimers[mac] = setInterval(() => api.identify(mac, true).catch(() => {}), 500);
  document.querySelector(`.identify-btn[data-mac="${mac}"]`)?.classList.add('holding');
};
window.nh_identStop = mac => {
  if (!identTimers[mac]) return;
  clearInterval(identTimers[mac]); delete identTimers[mac];
  api.identify(mac, false).catch(() => {});
  document.querySelector(`.identify-btn[data-mac="${mac}"]`)?.classList.remove('holding');
};

// ── Patch mode ────────────────────────────────────────────────────────────────

window.nh_togglePatchMode = () => {
  patchMode = !patchMode;
  const bar = document.getElementById('nh_patchBar');
  const btn = document.getElementById('nh_patchToggle');
  const wrap = document.getElementById('nh_tableWrap');
  if (patchMode) {
    bar.classList.add('visible'); btn.classList.add('active'); wrap.classList.add('patch-mode-active');
    const used = headsData.map(h => h.fixID).filter(id => id > 0);
    let start = 1; while (used.includes(start)) start++;
    document.getElementById('nh_patchStart').value = start; patchNextID = start; nh_updatePatchLabel();
    toast('Patch mode — click a head to assign Fix#');
  } else {
    bar.classList.remove('visible'); btn.classList.remove('active'); wrap.classList.remove('patch-mode-active');
  }
  nh_render();
};

window.nh_onStartChange    = () => { patchNextID = Math.max(1, parseInt(document.getElementById('nh_patchStart').value) || 1); nh_updatePatchLabel(); };
window.nh_resetPatchCounter = () => { patchNextID = Math.max(1, parseInt(document.getElementById('nh_patchStart').value) || 1); nh_updatePatchLabel(); toast('Counter reset to Fix#' + patchNextID); };

function nh_updatePatchLabel() {
  const el  = document.getElementById('nh_patchNextLabel'); if (el) el.textContent = patchNextID;
  const sel = document.getElementById('nh_patchSelCount');
  if (sel) { const n = selectedMACs.length; sel.textContent = n ? `(${n} selected → Fix#${patchNextID}–Fix#${patchNextID + n - 1})` : ''; }
}

window.nh_patchAssign = async mac => {
  const id   = patchNextID;
  const h    = headsData.find(x => x.mac === mac);
  const name = h?.name || `Fix #${id}`;
  await api.setFixID(mac, id).catch(() => {});
  if (h) h.fixID = id;
  await api.createFixture(id, name, mac).catch(() => api.updateFixture(id, { mac }).catch(() => {}));
  patchNextID++; nh_updatePatchLabel(); nh_render();
  const row = document.querySelector(`tr[data-mac="${mac}"]`);
  if (row) { row.classList.add('patch-patched-flash'); setTimeout(() => row.classList.remove('patch-patched-flash'), 500); }
  toast(`Fix#${id} → ${mac.slice(-8)} — next: Fix#${patchNextID}`);
};

window.nh_patchDone = async () => {
  const sorted = [...headsData].sort((a, b) => a.fixID - b.fixID).filter(h => selectedMACs.includes(h.mac));
  if (!sorted.length) { nh_togglePatchMode(); return; }
  const startId = patchNextID;
  await Promise.all(sorted.map(async (h, i) => {
    const id = startId + i;
    await api.setFixID(h.mac, id).catch(() => {}); h.fixID = id;
    await api.createFixture(id, h.name || `Fix #${id}`, h.mac).catch(() => api.updateFixture(id, { mac: h.mac }).catch(() => {}));
  }));
  patchNextID = startId + sorted.length; nh_updatePatchLabel(); nh_togglePatchMode(); nh_load();
  toast(`Patched ${sorted.length} head(s): Fix#${startId}–Fix#${startId + sorted.length - 1}`);
};

// ── Patch CRUD ────────────────────────────────────────────────────────────────

window.nh_patchUpdate = async (fixID, uni, addr) => {
  const p = anPatches.find(x => x.fixID === fixID); if (!p) return;
  const data = { universe: uni !== null ? uni : p.universe, startAddr: addr !== null ? addr : p.startAddr };
  await api.updatePatch(fixID, data).catch(e => { toast(e.message, 'err'); nh_load(); return; });
  p.universe = data.universe; p.startAddr = data.startAddr;
  if (headsData.some(h => h.fixID === fixID)) { patchAcks[fixID] = { status: 'pending', ts: Date.now() / 1000 }; nh_render(); }
  toast('Patch sent');
};

window.nh_patchDel = async fixID => {
  await api.deletePatch(fixID).catch(() => {});
  anPatches = anPatches.filter(p => p.fixID !== fixID); delete patchAcks[fixID]; nh_render(); toast('Patch removed');
};

window.nh_patchCreate = async fixID => {
  const used = anPatches.filter(p => p.universe === 0);
  let addr = 1;
  used.sort((a, b) => a.startAddr - b.startAddr).forEach(p => { if (p.startAddr <= addr && addr < p.startAddr + 7) addr = p.startAddr + 7; });
  if (addr + 6 > 512) { toast('Universe 0 full', 'err'); return; }
  await api.createPatch(fixID, 0, addr).catch(e => { toast(e.message, 'err'); return; });
  anPatches.push({ fixID, universe: 0, startAddr: addr });
  if (headsData.some(h => h.fixID === fixID)) patchAcks[fixID] = { status: 'pending', ts: Date.now() / 1000 };
  nh_render(); toast(`Patched Fix#${fixID} → Uni 0 Addr ${addr}`);
};

// ── Init ──────────────────────────────────────────────────────────────────────

nh_load();
setInterval(() => { if (!nhEditMode) nh_load(); }, 2000);
