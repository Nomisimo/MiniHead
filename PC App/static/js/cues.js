/**
 * cues.js — cue list, drag-reorder, edit-targets modal, sequencer selection.
 * Exports: loadCues(), saveCue()
 */
'use strict';

let seqSelectedIds = [];
let _editCueId     = null;
let _cDragId       = null;

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

async function loadCues() {
  const cues = await api.cues().catch(() => []);
  _renderCues(cues);
}

function _renderCues(cues) {
  const list = document.getElementById('cueList');
  if (!cues.length) {
    list.innerHTML = '<div style="font-family:var(--mono);font-size:10px;color:var(--text-dim);padding:8px 0;">No cues saved yet</div>';
    return;
  }
  list.innerHTML = '';
  cues.forEach(cue => {
    const inSeq = seqSelectedIds.includes(cue.id);
    const wr    = Math.min(255, cue.r + cue.w);
    const wg    = Math.min(255, cue.g + cue.w);
    const wb    = Math.min(255, cue.b + cue.w);
    const ftStr = cue.fixTargets?.length
      ? ' → ' + cue.fixTargets.map(id => id === 0 ? 'ALL' : '#' + id).join(' ')
      : '';

    const el = document.createElement('div');
    el.className   = 'cue-item' + (inSeq ? ' seq-selected' : '');
    el.draggable   = true;
    el.dataset.cueId = cue.id;
    el.innerHTML =
      `<div class="cue-drag" title="Drag to reorder">&#8942;</div>` +
      `<div class="cue-swatch" style="background:rgb(${wr},${wg},${wb})"></div>` +
      `<div class="cue-info">` +
        `<div class="cue-name">${escHtml(cue.name)}</div>` +
        `<div class="cue-meta">P:${cue.pan}° T:${cue.tilt}° W:${cue.w}${escHtml(ftStr)}</div>` +
      `</div>` +
      `<div class="cue-actions">` +
        `<button class="icon-btn" title="Edit targets" onclick="editCue(${cue.id},${JSON.stringify(cue.fixTargets||[0])})">&#9998;</button>` +
        `<button class="icon-btn" title="Add to sequence" onclick="toggleSeqCue(${cue.id})">+</button>` +
        `<button class="icon-btn" onclick="fireCue(${cue.id})">GO</button>` +
        `<button class="icon-btn del" onclick="deleteCue(${cue.id})">X</button>` +
      `</div>`;

    el.addEventListener('dragstart', e => { _cDragId = cue.id; e.dataTransfer.effectAllowed = 'move'; el.classList.add('dragging'); });
    el.addEventListener('dragend',   () => { el.classList.remove('dragging'); document.querySelectorAll('.cue-item.drag-over').forEach(x => x.classList.remove('drag-over')); });
    el.addEventListener('dragover',  e => { e.preventDefault(); e.dataTransfer.dropEffect = 'move'; el.classList.add('drag-over'); });
    el.addEventListener('dragleave', e => { if (!el.contains(e.relatedTarget)) el.classList.remove('drag-over'); });
    el.addEventListener('drop', async e => {
      e.preventDefault(); el.classList.remove('drag-over');
      if (_cDragId === cue.id) return;
      const items = [...document.querySelectorAll('.cue-item[data-cue-id]')];
      const order = items.map(x => +x.dataset.cueId);
      const fi = order.indexOf(_cDragId), ti = order.indexOf(cue.id);
      if (fi < 0 || ti < 0) return;
      order.splice(fi, 1); order.splice(ti, 0, _cDragId);
      await api.reorderCues(order).catch(() => {});
      loadCues();
    });

    list.appendChild(el);
  });
}

async function saveCue() {
  const name = document.getElementById('cueName').value.trim();
  if (!name) { toast('Enter a cue name', 'err'); return; }
  const v  = getValues();
  const ft = typeof nh_getSelectedFixIDs === 'function' ? nh_getSelectedFixIDs() : [];
  await api.createCue({ name, r: v.r, g: v.g, b: v.b, w: v.w, pan: v.pan, tilt: v.tilt, fixTargets: ft.length ? ft : [0] });
  document.getElementById('cueName').value = '';
  toast('Cue saved');
  loadCues();
}

async function fireCue(id) {
  await api.fireCue(id).catch(() => {});
  toast('Cue fired');
  // Sync faders to cue values
  const cues = await api.cues().catch(() => []);
  const cue  = cues.find(c => c.id === id);
  if (cue) {
    ['R','G','B','W'].forEach(ch => {
      document.getElementById('f'+ch).value = cue[ch.toLowerCase()];
      document.getElementById('v'+ch).value = cue[ch.toLowerCase()];
    });
    document.getElementById('fPan').value  = document.getElementById('vPan').value  = cue.pan;
    document.getElementById('fTilt').value = document.getElementById('vTilt').value = cue.tilt;
    if (typeof updatePreview === 'function') updatePreview();
  }
}

async function deleteCue(id) {
  seqSelectedIds = seqSelectedIds.filter(i => i !== id);
  await api.deleteCue(id).catch(() => {});
  toast('Cue deleted');
  loadCues();
}

function toggleSeqCue(id) {
  const idx = seqSelectedIds.indexOf(id);
  if (idx >= 0) seqSelectedIds.splice(idx, 1);
  else seqSelectedIds.push(id);
  loadCues();
  document.getElementById('seqStatus').textContent = seqSelectedIds.length + ' cue(s) in sequence';
}

// ── Edit modal ────────────────────────────────────────────────────────────────

async function editCue(id, curTargets) {
  _editCueId = id;
  const fixtures = await api.fixtures().catch(() => []);
  let html = `<label class="modal-check-row"><input type="checkbox" id="cue_all" ${curTargets.includes(0) ? 'checked' : ''}><span>ALL (broadcast to everyone)</span></label>`;
  fixtures.forEach(f => {
    if (f.id <= 0) return;
    html += `<label class="modal-check-row"><input type="checkbox" class="cue-fix-cb" value="${f.id}" ${curTargets.includes(f.id) ? 'checked' : ''}>${f.id} — ${escHtml(f.name||'?')}${f.online ? ' &#9679;' : ' &#9675;'}</label>`;
  });
  const poolIds = fixtures.map(f => f.id);
  const free    = curTargets.filter(id => id > 0 && !poolIds.includes(id));
  document.getElementById('cueEditList').innerHTML = html;
  document.getElementById('cueEditFree').value     = free.join(', ');
  document.getElementById('cueEditModal').classList.add('open');
}

function cueEditClose() {
  document.getElementById('cueEditModal').classList.remove('open');
  _editCueId = null;
}

async function cueEditSave() {
  if (!_editCueId) return;
  let ft = [];
  if (document.getElementById('cue_all').checked) {
    ft = [0];
  } else {
    document.querySelectorAll('.cue-fix-cb:checked').forEach(cb => ft.push(+cb.value));
    document.getElementById('cueEditFree').value.split(',').forEach(s => {
      const n = parseInt(s); if (n > 0 && !ft.includes(n)) ft.push(n);
    });
  }
  if (!ft.length) ft = [0];
  await api.updateTargets(_editCueId, ft).catch(() => {});
  cueEditClose();
  toast('Targets updated');
  loadCues();
}
