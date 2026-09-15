#pragma once

// ── Active theme CSS ─────────────────────────────────────────────
// Served at GET /theme. Edit :root to retheme without touching layout.

const char THEME_CSS[] PROGMEM = R"=====(
:root{
  --bg:#0d0d11;--surface:#141418;--surface2:#1c1c23;
  --border:#2e2e3a;--accent:#00c8f0;--accent2:#f07040;
  --accent3:#8855dd;--text:#d8d8e8;--text-dim:#60607a;
  --success:#20c070;--danger:#e04848;
  --mono:ui-monospace,'Courier New',monospace;--sans:'Inter',system-ui,-apple-system,sans-serif;
}
body{background:var(--bg);color:var(--text);font-family:var(--sans);font-size:14px;}
header{padding:10px 16px;background:var(--surface);border-bottom:1px solid var(--border);}
.logo{font-family:var(--mono);font-size:14px;color:var(--accent);letter-spacing:1px;}
.logo span{color:var(--text-dim);}
.connection-bar{gap:8px;font-family:var(--mono);font-size:11px;}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--success);animation:pulse 2s infinite;}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.4}}
.ip-label{color:var(--accent);}
.btn{padding:6px 14px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:12px;cursor:pointer;border-radius:3px;touch-action:manipulation;transition:border-color 0.1s,color 0.1s,opacity 0.1s;}
.btn:hover{border-color:var(--text-dim);}
.btn:active{opacity:0.65;}
.btn.primary{border-color:var(--accent);color:var(--accent);}
.btn.danger{border-color:var(--danger);color:var(--danger);}
.btn.success{border-color:var(--success);color:var(--success);}
.btn.active{border-color:var(--accent3);color:var(--accent3);background:rgba(136,85,221,0.1);}
.panel{background:var(--bg);padding:16px 20px;border-bottom:1px solid var(--border);}
.panel:last-child{border-bottom:none;}
.panel-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid var(--border);}
hr{border:none;border-top:1px solid var(--border);margin:0 0 14px;}
.main{gap:1px;background:var(--border);}
@media(min-width:900px){.col-right{border-left:1px solid var(--border);}}
.faders-row{gap:12px;}
.fader-group{gap:8px;}
.fader-label{font-family:var(--mono);font-size:10px;color:var(--text-dim);letter-spacing:1px;}
input[type=range]{-webkit-appearance:none;appearance:none;background:transparent;}
input[type=range].vertical{-webkit-appearance:none;appearance:none;}
input[type=range].vertical::-webkit-slider-runnable-track{width:3px;background:var(--text-dim);border-radius:2px;}
input[type=range].vertical::-webkit-slider-thumb{-webkit-appearance:none;width:28px;height:12px;border-radius:2px;background:var(--accent);border:1px solid var(--bg);}
.fader-r input[type=range]::-webkit-slider-thumb{background:#e04040;}
.fader-g input[type=range]::-webkit-slider-thumb{background:#30c040;}
.fader-b input[type=range]::-webkit-slider-thumb{background:#4080f0;}
.fader-w input[type=range]::-webkit-slider-thumb{background:#c8c8d8;}
input[type=number]{width:44px;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:4px 2px;font-family:var(--mono);font-size:12px;border-radius:3px;outline:none;text-align:center;transition:border-color 0.1s;}
input[type=number]:focus{border-color:var(--accent);}
.preview-row{margin-top:14px;}
.led-preview{width:56px;height:56px;border-radius:4px;border:1px solid var(--border);background:#000;transition:background 0.08s;}
.motion-row{gap:10px;margin-bottom:12px;}
.motion-label{font-family:var(--mono);font-size:11px;color:var(--text-dim);letter-spacing:1px;}
input[type=range].horizontal{-webkit-appearance:none;appearance:none;width:100%;height:4px;background:var(--surface2);border-radius:2px;outline:none;}
input[type=range].horizontal::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:var(--accent2);border:2px solid var(--bg);}
.rainbow-btn{width:100%;padding:10px;font-size:12px;font-family:var(--mono);background:linear-gradient(90deg,#ff000022,#ffff0022,#00ff0022,#00ffff22,#0000ff22,#ff00ff22);border:1px solid var(--border);color:var(--text);touch-action:manipulation;border-radius:3px;}
.rainbow-btn.active{background:linear-gradient(90deg,#ff0000aa,#ffff00aa,#00ff00aa,#00ffffaa,#0000ffaa,#ff00ffaa);animation:rainbow-shift 2s linear infinite;color:#fff;border-color:transparent;}
.demo-btn{width:100%;padding:10px;font-size:12px;font-family:var(--mono);background:linear-gradient(90deg,#00ffff14,#0066ff14,#cc00ff14,#00ffff14);border:1px solid var(--border);color:var(--text);touch-action:manipulation;border-radius:3px;}
.demo-btn.active{background:linear-gradient(90deg,#00ffffaa,#0066ffaa,#cc00ffaa,#00ffffaa);animation:demo-shift 3s linear infinite;color:#fff;border-color:transparent;}
@keyframes rainbow-shift{0%{filter:hue-rotate(0deg)}100%{filter:hue-rotate(360deg)}}
@keyframes demo-shift{0%{filter:hue-rotate(0deg)}100%{filter:hue-rotate(360deg)}}
.cue-list{gap:4px;margin-bottom:10px;}
.cue-list::-webkit-scrollbar{width:4px;}
.cue-list::-webkit-scrollbar-track{background:var(--bg);}
.cue-list::-webkit-scrollbar-thumb{background:var(--border);border-radius:2px;}
.cue-item{gap:10px;padding:8px 10px;background:var(--surface2);border:1px solid var(--border);border-radius:3px;cursor:default;transition:border-color 0.1s;}
.cue-item.seq-selected{border-color:var(--accent3);background:rgba(136,85,221,0.1);}
.cue-item.drag-over{border-color:var(--accent);background:rgba(0,200,240,0.08);}
.cue-item.dragging{opacity:0.4;}
.cue-drag{cursor:grab;color:var(--text-dim);font-size:14px;padding:0 2px;flex-shrink:0;user-select:none;line-height:1;}
.cue-swatch{width:22px;height:22px;border-radius:3px;flex-shrink:0;border:1px solid var(--border);}
.cue-name{font-size:13px;font-weight:600;}
.cue-meta{font-family:var(--mono);font-size:10px;color:var(--text-dim);margin-top:2px;}
.cue-actions{gap:4px;}
.icon-btn{width:28px;height:28px;border:1px solid var(--border);background:var(--surface2);color:var(--text-dim);border-radius:3px;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;touch-action:manipulation;transition:border-color 0.1s,color 0.1s;}
.icon-btn:active{opacity:0.65;}
.save-cue-form{gap:8px;}
input[type=text]{flex:1;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:6px 10px;font-family:var(--mono);font-size:12px;border-radius:3px;outline:none;transition:border-color 0.1s;}
input[type=text]:focus{border-color:var(--accent);}
.seq-controls{gap:8px;}
.seq-row{gap:6px;}
.seq-label{font-family:var(--mono);font-size:10px;color:var(--text-dim);}
.seq-buttons{gap:6px;margin-top:8px;}
.seq-buttons .btn{padding:8px;text-align:center;}
.seq-hint{font-size:11px;color:var(--text-dim);margin-bottom:8px;}
.future-placeholder{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;min-height:120px;border:1px dashed var(--border);border-radius:3px;color:var(--text-dim);font-family:var(--mono);font-size:11px;letter-spacing:1px;}
.serial-row{gap:8px;}
#cueEditModal{background:rgba(0,0,0,0.75);}
.modal-box{background:var(--surface);border:1px solid var(--border);border-radius:4px;padding:20px;}
.modal-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:14px;padding-bottom:8px;border-bottom:1px solid var(--border);}
.modal-check-row{gap:8px;margin-bottom:6px;font-family:var(--mono);font-size:11px;}
.modal-check-row input{accent-color:var(--accent3);width:14px;height:14px;flex-shrink:0;}
.modal-footer{gap:8px;margin-top:14px;}
#toast{padding:8px 16px;font-family:var(--mono);font-size:12px;border-radius:3px;background:var(--surface2);border:1px solid var(--border);color:var(--text);}
#toast.ok{border-color:var(--success);color:var(--success);}
#toast.err{border-color:var(--danger);color:var(--danger);}
@media(min-width:900px){#toast{left:auto;width:auto;}}
#artnet-bar{gap:10px;padding:5px 16px;background:rgba(32,192,112,0.08);border-bottom:1px solid rgba(32,192,112,0.3);font-family:var(--mono);font-size:11px;color:var(--success);letter-spacing:1px;}
#artnet-bar-dot{width:7px;height:7px;border-radius:50%;background:var(--success);animation:pulse 1.2s infinite;flex-shrink:0;}
#artnet-live{color:var(--text);letter-spacing:0;font-size:11px;opacity:0.8;}
#artnet-bar-count{color:var(--text-dim);margin-left:auto;white-space:nowrap;}
)=====";
