#pragma once

// ── Active theme CSS ─────────────────────────────────────────────
// Served at GET /theme. Edit :root to retheme without touching layout.

const char THEME_CSS[] PROGMEM = R"=====(
:root {
  --bg:#0a0a0f; --surface:#13131a; --surface2:#1c1c26;
  --border:#2a2a3a; --accent:#00e5ff; --accent2:#ff6b35;
  --accent3:#a855f7; --text:#e0e0f0; --text-dim:#6b6b8a;
  --success:#22c55e; --danger:#ef4444;
  --mono:'Share Tech Mono',monospace; --sans:'Barlow',sans-serif;
}
body{background:var(--bg);color:var(--text);font-family:var(--sans);font-size:14px;}
body::before{content:'';position:fixed;inset:0;background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,229,255,0.015) 2px,rgba(0,229,255,0.015) 4px);pointer-events:none;z-index:1000;}
header{padding:12px 16px;background:var(--surface);border-bottom:1px solid var(--border);}
.logo{font-family:var(--mono);font-size:16px;color:var(--accent);letter-spacing:2px;text-transform:uppercase;}
.logo span{color:var(--text-dim);}
.connection-bar{gap:8px;font-family:var(--mono);font-size:11px;}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--success);box-shadow:0 0 8px var(--success);animation:pulse 2s infinite;}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
.ip-label{color:var(--accent);}
.btn{padding:9px 18px;border:1px solid var(--border);background:var(--surface2);color:var(--text);font-family:var(--mono);font-size:13px;cursor:pointer;border-radius:4px;letter-spacing:1px;text-transform:uppercase;touch-action:manipulation;}
.btn:active{opacity:0.7;}
.btn.primary{border-color:var(--accent);color:var(--accent);background:rgba(0,229,255,0.08);}
.btn.danger{border-color:var(--danger);color:var(--danger);}
.btn.success{border-color:var(--success);color:var(--success);}
.btn.active{background:rgba(168,85,247,0.2);border-color:var(--accent3);color:var(--accent3);}
.panel{background:var(--bg);padding:20px 24px;border-bottom:1px solid var(--border);}
.panel:last-child{border-bottom:none;}
.panel-title{font-family:var(--mono);font-size:13px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:10px;}
hr{border:none;border-top:1px solid var(--border);margin:0 0 18px;}
.main{gap:1px;background:var(--border);}
@media(min-width:900px){.col-right{border-left:1px solid var(--border);}}
.faders-row{gap:16px;}
.fader-group{gap:10px;}
.fader-label{font-family:var(--mono);font-size:10px;color:var(--text-dim);letter-spacing:2px;}
input[type=range]{-webkit-appearance:none;appearance:none;background:transparent;}
input[type=range].vertical::-webkit-slider-thumb{-webkit-appearance:none;width:32px;height:16px;border-radius:3px;background:var(--accent);box-shadow:0 0 12px var(--accent);border:2px solid var(--bg);}
.fader-r input[type=range]::-webkit-slider-thumb{background:#ff4444;box-shadow:0 0 12px #ff4444;}
.fader-g input[type=range]::-webkit-slider-thumb{background:#44ff44;box-shadow:0 0 12px #44ff44;}
.fader-b input[type=range]::-webkit-slider-thumb{background:#4488ff;box-shadow:0 0 12px #4488ff;}
.fader-w input[type=range]::-webkit-slider-thumb{background:#ffffff;box-shadow:0 0 12px #ffffff;}
input[type=number]{width:44px;background:#ffffff17;border:1px solid #ffffff17;color:var(--text);padding:7px 4px;font-family:var(--mono);font-size:13px;border-radius:5px;outline:none;text-align:center;}
.preview-row{margin-top:16px;}
.led-preview{width:64px;height:64px;border-radius:50%;border:2px solid var(--border);transition:background 0.1s,box-shadow 0.1s;background:#000;}
.motion-row{gap:14px;margin-bottom:14px;}
.motion-label{font-family:var(--mono);font-size:11px;color:var(--text-dim);letter-spacing:2px;}
input[type=range].horizontal{-webkit-appearance:none;appearance:none;width:100%;height:6px;background:var(--surface2);border-radius:3px;outline:none;}
input[type=range].horizontal::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;border-radius:50%;background:var(--accent2);box-shadow:0 0 10px var(--accent2);border:2px solid var(--bg);}
.rainbow-btn{width:100%;padding:14px;font-size:13px;letter-spacing:3px;background:linear-gradient(90deg,#ff000033,#ffff0033,#00ff0033,#00ffff33,#0000ff33,#ff00ff33);border:1px solid var(--border);color:var(--text);touch-action:manipulation;}
.rainbow-btn.active{background:linear-gradient(90deg,#ff0000aa,#ffff00aa,#00ff00aa,#00ffffaa,#0000ffaa,#ff00ffaa);animation:rainbow-shift 2s linear infinite;color:#fff;border-color:transparent;}
@keyframes rainbow-shift{0%{filter:hue-rotate(0deg)}100%{filter:hue-rotate(360deg)}}
.demo-btn{width:100%;padding:14px;font-size:13px;letter-spacing:3px;background:linear-gradient(90deg,#00ffff22,#0066ff22,#cc00ff22,#00ffff22);border:1px solid var(--border);color:var(--text);touch-action:manipulation;}
.demo-btn.active{background:linear-gradient(90deg,#00ffffaa,#0066ffaa,#cc00ffaa,#00ffffaa);animation:demo-shift 3s linear infinite;color:#fff;border-color:transparent;}
@keyframes demo-shift{0%{filter:hue-rotate(0deg)}100%{filter:hue-rotate(360deg)}}
.cue-list{gap:6px;margin-bottom:12px;}
.cue-list::-webkit-scrollbar{width:4px;}
.cue-list::-webkit-scrollbar-thumb{background:var(--border);border-radius:2px;}
.cue-item{gap:12px;padding:10px 12px;background:var(--surface2);border:2px solid var(--border);border-radius:5px;cursor:default;}
.cue-item.seq-selected{border-color:var(--accent3);background:rgba(168,85,247,0.1);}
.cue-item.drag-over{border-color:var(--accent);background:rgba(99,102,241,0.15);}
.cue-item.dragging{opacity:0.4;}
.cue-drag{cursor:grab;color:var(--text-dim);font-size:16px;padding:0 2px;flex-shrink:0;user-select:none;line-height:1;}
.cue-swatch{width:28px;height:28px;border-radius:4px;flex-shrink:0;border:1px solid rgba(255,255,255,0.1);}
.cue-name{font-size:13px;font-weight:600;}
.cue-meta{font-family:var(--mono);font-size:10px;color:var(--text-dim);margin-top:2px;}
.cue-actions{gap:4px;}
.icon-btn{width:34px;height:34px;border:1px solid var(--border);background:transparent;color:var(--text-dim);border-radius:3px;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:14px;touch-action:manipulation;}
.save-cue-form{gap:10px;}
input[type=text]{flex:1;background:#ffffff17;border:1px solid #ffffff17;color:var(--text);padding:8px 10px;font-family:var(--mono);font-size:13px;border-radius:5px;outline:none;}
.seq-controls{gap:10px;}
.seq-row{gap:8px;}
.seq-label{font-family:var(--mono);font-size:10px;color:var(--text-dim);}
.seq-buttons{gap:8px;margin-top:10px;}
.seq-buttons .btn{padding:11px;text-align:center;}
.seq-hint{font-size:11px;color:var(--text-dim);margin-bottom:10px;}
.future-placeholder{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;min-height:120px;border:1px dashed var(--border);border-radius:4px;color:var(--text-dim);font-family:var(--mono);font-size:11px;letter-spacing:2px;}
.serial-row{gap:8px;}
#cueEditModal{background:rgba(0,0,0,0.82);}
.modal-box{background:var(--surface);border:1px solid var(--border);border-radius:6px;padding:20px;}
.modal-title{font-family:var(--mono);font-size:11px;color:var(--accent);letter-spacing:3px;text-transform:uppercase;margin-bottom:14px;padding-bottom:8px;border-bottom:1px solid var(--border);}
.modal-check-row{gap:8px;margin-bottom:7px;font-family:var(--mono);font-size:11px;}
.modal-check-row input{accent-color:var(--accent3);width:15px;height:15px;flex-shrink:0;}
.modal-footer{gap:8px;margin-top:14px;}
#toast{padding:10px 18px;font-family:var(--mono);font-size:12px;border-radius:4px;background:var(--surface2);border:1px solid var(--border);color:var(--text);}
#toast.ok{border-color:var(--success);color:var(--success);}
#toast.err{border-color:var(--danger);color:var(--danger);}
@media(min-width:900px){#toast{left:auto;width:auto;}}
#artnet-bar{gap:10px;padding:6px 16px;background:rgba(34,197,94,0.10);border-bottom:1px solid rgba(34,197,94,0.4);font-family:var(--mono);font-size:11px;color:var(--success);letter-spacing:1px;}
#artnet-bar-dot{width:8px;height:8px;border-radius:50%;background:var(--success);box-shadow:0 0 6px var(--success);animation:pulse 1s infinite;flex-shrink:0;}
#artnet-live{color:var(--text);letter-spacing:0;font-size:11px;opacity:0.85;}
#artnet-bar-count{color:var(--text-dim);margin-left:auto;white-space:nowrap;}
)=====";
