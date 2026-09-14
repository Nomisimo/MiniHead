/**
 * artnet.js — Art-Net status bar + DMX patch grid panel.
 */
'use strict';

// ── Status bar ────────────────────────────────────────────────────────────────
let _artnetWasActive = false;

async function an_pollStatus() {
  const d = await api.artnetStatus().catch(() => null);
  if (!d) return;
  const active = !!d.active;
  if (active !== _artnetWasActive) {
    _artnetWasActive = active;
    document.getElementById('artnet-bar').classList.toggle('visible', active);
    ['.area-light','.area-motion','.area-rainbow','.area-serial','.area-sequencer'].forEach(sel => {
      document.querySelector(sel)?.classList.toggle('artnet-locked', active);
    });
  }
  if (active) {
    document.getElementById('artnet-live').textContent =
      `R:${d.r} G:${d.g} B:${d.b} W:${d.w}  PAN:${d.pan}° TILT:${d.tilt}°`;
    document.getElementById('artnet-bar-count').textContent = d.patchCount + ' fix';
  } else {
    document.getElementById('artnet-live').textContent    = '';
    document.getElementById('artnet-bar-count').textContent = '';
  }
}

setInterval(an_pollStatus, 2000);

// ── DMX Patch Grid ────────────────────────────────────────────────────────────
(function () {
  const DMX_FP = 7, COLS = 32, ROWS = 16;
  const FIX_COLORS = [
    '#000080','#800000','#008000','#800080','#808000','#008080',
    '#0000c0','#c00000','#00c000','#c000c0','#c0c000','#00c0c0',
    '#404080','#804040','#408040','#406080',
  ];
  const fixColor = id => FIX_COLORS[(id - 1) % FIX_COLORS.length];

  let pg_uni     = 0;
  let pg_patches = [];

  async function pg_load() {
    pg_patches = await api.artnetPatches().catch(() => []);
    pg_buildTabs(); pg_render();
  }

  function pg_buildTabs() {
    const unis = [...new Set([0, ...pg_patches.map(p => p.universe)])].sort((a, b) => a - b);
    const c = document.getElementById('pg_uniTabs');
    if (!c) return;
    c.innerHTML = '';
    unis.forEach(u => {
      const b = document.createElement('button');
      b.className = 'net-btn' + (u === pg_uni ? ' active' : '');
      b.textContent = 'Uni ' + u;
      b.onclick = () => { pg_uni = u; pg_buildTabs(); pg_render(); };
      c.appendChild(b);
    });
    const next = (unis[unis.length - 1] ?? -1) + 1;
    const ab = document.createElement('button');
    ab.className = 'net-btn'; ab.textContent = '+ Uni ' + next;
    ab.onclick = () => { pg_uni = next; pg_buildTabs(); pg_render(); };
    c.appendChild(ab);
  }

  function pg_render() {
    const grid = document.getElementById('pg_grid'); if (!grid) return;
    const occ  = {};
    pg_patches.filter(p => p.universe === pg_uni).forEach(p => {
      for (let i = 0; i < DMX_FP; i++) { const a = p.startAddr + i; if (a >= 1 && a <= 512) occ[a] = { fixID: p.fixID, isStart: i === 0 }; }
    });
    grid.innerHTML = '';
    for (let row = 0; row < ROWS; row++) {
      const rd = document.createElement('div'); rd.className = 'pg-row';
      for (let col = 0; col < COLS; col++) {
        const addr = row * COLS + col + 1;
        const cell = document.createElement('div'); cell.className = 'pg-cell';
        const o = occ[addr];
        if (o) {
          cell.style.background = fixColor(o.fixID); cell.style.color = '#fff';
          if (o.isStart) cell.textContent = o.fixID <= 99 ? 'F' + o.fixID : 'F?';
          cell.title = `Fix#${o.fixID} ch${addr}${o.isStart ? ' [start]' : ''} — click to remove`;
          cell.onclick = () => api.deletePatch(o.fixID).then(() => { pg_load(); toast('Patch removed'); });
        } else {
          cell.title = `ch${addr} (empty) — click to patch`;
          cell.onclick = async () => {
            const fid = parseInt(document.getElementById('pg_fixSel').value) || 0;
            if (!fid) { toast('Enter Fix# first', 'err'); return; }
            await api.createPatch(fid, pg_uni, addr).catch(e => toast(e.message, 'err'));
            toast(`Fix#${fid} → Uni${pg_uni} ch${addr}`);
            pg_load();
          };
        }
        rd.appendChild(cell);
      }
      grid.appendChild(rd);
    }
    const up = pg_patches.filter(p => p.universe === pg_uni);
    const info = document.getElementById('pg_info');
    if (info) info.textContent = `${up.length} fix • ${up.length * DMX_FP}/512 ch`;
  }

  window.pg_clearAll = async function () {
    const fids = pg_patches.filter(p => p.universe === pg_uni).map(p => p.fixID);
    if (!fids.length) { toast('Uni ' + pg_uni + ' already empty'); return; }
    await Promise.all(fids.map(fid => api.deletePatch(fid))).catch(() => {});
    toast(`Cleared ${fids.length} patch(es) from Uni ${pg_uni}`);
    pg_load();
  };

  window.pg_poll = async function () {
    const d = await api.pollNodes().catch(() => null);
    if (d) toast(`Found ${d.nodes.length} node(s)`);
    pg_load();
    if (typeof nh_load === 'function') nh_load();
  };

  window.pg_reload = pg_load;
  pg_load();
  setInterval(pg_load, 5000);
})();
