/**
 * controls.js — faders, rainbow/demo/blackout, animation speed.
 * Exports: getValues(), onFader(), onMotion(), onSpeed(),
 *          toggleRainbow(), toggleDemo(), blackout()
 */
'use strict';

let rainbowActive = false;
let demoActive    = false;
let _sendTimer    = null;
let _speedTimer   = null;

function getValues() {
  return {
    r:    +document.getElementById('fR').value,
    g:    +document.getElementById('fG').value,
    b:    +document.getElementById('fB').value,
    w:    +document.getElementById('fW').value,
    pan:  +document.getElementById('fPan').value,
    tilt: +document.getElementById('fTilt').value,
  };
}

function updatePreview() {
  const v  = getValues();
  const wr = Math.min(255, v.r + v.w);
  const wg = Math.min(255, v.g + v.w);
  const wb = Math.min(255, v.b + v.w);
  const hex = `rgb(${wr},${wg},${wb})`;
  const br  = (wr + wg + wb) / 3;
  const p   = document.getElementById('ledPreview');
  p.style.background = hex;
  p.style.boxShadow  = br > 10 ? `0 0 ${20 + br / 4}px ${8 + br / 8}px ${hex}` : 'none';
}

function onFader() {
  const v = getValues();
  document.getElementById('vR').value = v.r;
  document.getElementById('vG').value = v.g;
  document.getElementById('vB').value = v.b;
  document.getElementById('vW').value = v.w;
  updatePreview();
  _debounceSend();
}

function onMotion() {
  const v = getValues();
  document.getElementById('vPan').value  = v.pan;
  document.getElementById('vTilt').value = v.tilt;
  _debounceSend();
}

function _debounceSend() {
  clearTimeout(_sendTimer);
  _sendTimer = setTimeout(_sendCurrent, 40);
}

function _sendCurrent() {
  const v   = getValues();
  const cmd = `R:${v.r},G:${v.g},B:${v.b},W:${v.w},PAN:${v.pan},TILT:${v.tilt}`;
  const macs = typeof nh_getSelectedMACs === 'function' ? nh_getSelectedMACs() : [];
  api.send(cmd, macs.length ? macs : undefined).catch(() => {});
}

function sendRaw() {
  const cmd = document.getElementById('cmdInput').value.trim();
  if (!cmd) return;
  api.send(cmd).catch(() => {});
  toast('Sent: ' + cmd);
}

function toggleRainbow() {
  rainbowActive = !rainbowActive;
  if (rainbowActive) { demoActive = false; document.getElementById('demoBtn').classList.remove('active'); }
  document.getElementById('rainbowBtn').classList.toggle('active', rainbowActive);
  api.rainbow(rainbowActive).catch(() => {});
  toast(rainbowActive ? 'Rainbow ON' : 'Rainbow OFF');
}

function toggleDemo() {
  demoActive = !demoActive;
  if (demoActive) { rainbowActive = false; document.getElementById('rainbowBtn').classList.remove('active'); }
  document.getElementById('demoBtn').classList.toggle('active', demoActive);
  api.demo(demoActive).catch(() => {});
  toast(demoActive ? 'Demo ON' : 'Demo OFF');
}

function blackout() {
  rainbowActive = false; demoActive = false;
  document.getElementById('rainbowBtn').classList.remove('active');
  document.getElementById('demoBtn').classList.remove('active');
  api.blackout().catch(() => {});
  toast('Blackout');
}

function onSpeed() {
  const val = +document.getElementById('fSpeed').value;
  document.getElementById('vSpeed').textContent = (val / 100).toFixed(1) + '×';
  clearTimeout(_speedTimer);
  _speedTimer = setTimeout(() => api.speed(val / 100).catch(() => {}), 40);
}

updatePreview();
