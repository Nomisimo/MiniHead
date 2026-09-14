/**
 * main.js — app bootstrap: status bar, init, toast helper.
 * Must be loaded last (after api.js, controls.js, cues.js, sequencer.js, artnet.js, heads.js).
 */
'use strict';

// ── Toast ─────────────────────────────────────────────────────────────────────
function toast(msg, type) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className   = 'show ' + (type || 'ok');
  clearTimeout(el._t);
  el._t = setTimeout(() => { el.className = ''; }, 2500);
}

// ── Status bar ────────────────────────────────────────────────────────────────
(async () => {
  const [st, ver] = await Promise.allSettled([api.status(), api.version()]);
  if (st.status === 'fulfilled' && st.value.ip)
    document.getElementById('ipLabel').textContent = st.value.ip;
  if (ver.status === 'fulfilled' && ver.value.version)
    document.getElementById('appVersion').textContent = '[' + ver.value.version + ']';
})();

// ── Serial input ──────────────────────────────────────────────────────────────
document.getElementById('cmdInput')?.addEventListener('keydown', e => {
  if (e.key === 'Enter') sendRaw();
});

// ── Initial data load ─────────────────────────────────────────────────────────
loadCues();
