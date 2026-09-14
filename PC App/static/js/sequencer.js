/**
 * sequencer.js — start/stop UI, reads seqSelectedIds from cues.js.
 */
'use strict';

async function startSequencer() {
  if (!seqSelectedIds.length) { toast('Select cues first', 'err'); return; }
  const interval_ms = +document.getElementById('seqInterval').value;
  const loop        = document.getElementById('seqLoop').checked;
  await api.seqStart(seqSelectedIds, interval_ms, loop).catch(() => {});
  document.getElementById('seqStatus').textContent = `Running — ${seqSelectedIds.length} cues`;
  document.getElementById('seqStartBtn').classList.add('active');
  toast('Sequencer started');
}

async function stopSequencer() {
  await api.seqStop().catch(() => {});
  document.getElementById('seqStatus').textContent = 'Sequencer idle';
  document.getElementById('seqStartBtn').classList.remove('active');
  toast('Sequencer stopped');
}
