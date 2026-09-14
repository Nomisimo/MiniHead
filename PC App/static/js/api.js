/**
 * api.js — typed fetch wrappers for all backend endpoints.
 * Imported by every module; never touches the DOM.
 */
'use strict';

async function _req(method, path, body) {
  const opts = { method };
  if (body !== undefined) {
    opts.headers = { 'Content-Type': 'application/json' };
    opts.body    = JSON.stringify(body);
  }
  const r = await fetch(path, opts);
  if (!r.ok) {
    const msg = await r.json().catch(() => ({ error: r.statusText }));
    throw new Error(msg.error || r.statusText);
  }
  return r.json();
}

const api = {
  // ── System ───────────────────────────────────────────────────────────────
  status:  ()      => _req('GET',  '/api/status'),
  version: ()      => _req('GET',  '/api/version'),

  // ── Heads ─────────────────────────────────────────────────────────────────
  heads:           ()          => _req('GET',  '/api/heads'),
  setFixID:        (mac, id)   => _req('POST', `/api/heads/${mac}/fixid`,    { fixID: id }),
  setName:         (mac, name) => _req('POST', `/api/heads/${mac}/name`,     { name }),
  identify:        (mac, on)   => _req('POST', `/api/heads/${mac}/identify`, { on }),

  // ── Fixtures ──────────────────────────────────────────────────────────────
  fixtures:        ()           => _req('GET',    '/api/fixtures'),
  createFixture:   (id,name,mac)=> _req('POST',   '/api/fixtures',    { id, name, mac }),
  updateFixture:   (id, data)   => _req('PUT',    `/api/fixtures/${id}`, data),
  deleteFixture:   (id)         => _req('DELETE',  `/api/fixtures/${id}`),

  // ── Cues ──────────────────────────────────────────────────────────────────
  cues:            ()          => _req('GET',    '/api/cues'),
  createCue:       (data)      => _req('POST',   '/api/cues',                     data),
  deleteCue:       (id)        => _req('DELETE', `/api/cues/${id}`),
  fireCue:         (id)        => _req('POST',   `/api/cues/${id}/fire`),
  updateTargets:   (id, tgts)  => _req('PUT',    `/api/cues/${id}/targets`, { fixTargets: tgts }),
  reorderCues:     (order)     => _req('PUT',    '/api/cues/reorder',       { order }),

  // ── Sequencer ─────────────────────────────────────────────────────────────
  seqStart:  (cue_ids, interval_ms, loop) =>
    _req('POST', '/api/sequencer/start', { cue_ids, interval_ms, loop }),
  seqStop:   ()  => _req('POST', '/api/sequencer/stop'),
  seqStatus: ()  => _req('GET',  '/api/sequencer/status'),

  // ── Art-Net ───────────────────────────────────────────────────────────────
  artnetStatus:   ()                       => _req('GET',  '/api/artnet/status'),
  artnetPatches:  ()                       => _req('GET',  '/api/artnet/patch'),
  artnetPatchAck: ()                       => _req('GET',  '/api/artnet/patch/ack'),
  createPatch:    (fixID,universe,startAddr)=> _req('POST', '/api/artnet/patch', { fixID, universe, startAddr }),
  updatePatch:    (fid, data)              => _req('PUT',   `/api/artnet/patch/${fid}`, data),
  deletePatch:    (fid)                    => _req('DELETE',`/api/artnet/patch/${fid}`),
  clearPatches:   ()                       => _req('DELETE','/api/artnet/patch'),
  pollNodes:      ()                       => _req('POST', '/api/artnet/poll'),

  // ── Commands ──────────────────────────────────────────────────────────────
  send:     (command, targets) => _req('POST', '/api/send',            { command, targets }),
  rainbow:  (on)               => _req('POST', '/api/control/rainbow', { on }),
  demo:     (on)               => _req('POST', '/api/control/demo',    { on }),
  blackout: ()                 => _req('POST', '/api/control/blackout'),
  speed:    (speed)            => _req('POST', '/api/control/speed',   { speed }),
};
