#!/usr/bin/env python3
"""
artnet_test.py — Art-Net ArtDmx test sender  [v2 — Browser UI]

Serves a browser UI at http://localhost:8765 AND keeps the
live terminal protocol monitor running in the background.

Usage:
  pip install flask
  python3 artnet_test.py [TARGET_IP] [UNIVERSE] [START_ADDR]

  Defaults: broadcast (255.255.255.255), universe 0, start addr 1

Terminal keys (also work while browser is open):
  q       quit
  SPACE   toggle demo / manual
  r/R g/G b/B w/W  colour ±10
  m/M     master ±10
  p/P t/T pan/tilt ±10
  0       blackout
  f       full white
  h       highlight
  n       rainbow toggle
  s       snapshot
"""

import socket, struct, sys, time, threading, math, os
import termios, tty
from flask import Flask, request, jsonify

# ── Config ────────────────────────────────────────────────────────
TARGET_IP   = sys.argv[1] if len(sys.argv) > 1 else "255.255.255.255"
UNIVERSE    = int(sys.argv[2]) if len(sys.argv) > 2 else 0
START_ADDR  = int(sys.argv[3]) if len(sys.argv) > 3 else 1   # 1-based
# Optional: --bind <local_ip> forces the socket onto a specific interface.
# Useful on macOS with VPN/Docker (prevents packets routing to wrong interface).
BIND_IP     = ""
for _i, _a in enumerate(sys.argv):
    if _a == "--bind" and _i + 1 < len(sys.argv):
        BIND_IP = sys.argv[_i + 1]
ARTNET_PORT = 6454
SEND_RATE   = 40      # packets/s
UI_PORT     = 8765

# ── Interface detection ────────────────────────────────────────────
def get_local_ip_for(target_ip: str) -> str:
    """Return the local IP address the OS would use to reach target_ip.
    On macOS with active VPN or Docker, this ensures packets leave on the
    correct LAN interface rather than being swallowed by a tunnel."""
    probe = "8.8.8.8" if target_ip in ("255.255.255.255", "<broadcast>") else target_ip
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect((probe, 1))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return ""

# ── DMX channel layout (matches firmware footprint) ───────────────
CH_MASTER = 0
CH_RED    = 1
CH_GREEN  = 2
CH_BLUE   = 3
CH_WHITE  = 4
CH_PAN    = 5
CH_TILT   = 6
DMX_FP    = 7

# ── State ─────────────────────────────────────────────────────────
dmx          = [0] * 512
dmx[CH_MASTER] = 255
dmx_last_sent = [None] * 512   # None = force send on first tick
seq_num      = 0
pkt_count    = 0
last_send_reason = "—"   # "change" or "heartbeat"
demo_mode    = False
rainbow_mode = False
rainbow_hue  = 0.0
demo_t       = 0.0
running      = True
lock         = threading.Lock()
snapshots    = []

# Target is mutable (browser can change it)
target = {"ip": TARGET_IP, "universe": UNIVERSE, "startAddr": START_ADDR}

def clamp(v): return max(0, min(255, int(v)))
def base():   return target["startAddr"] - 1

# ── Art-Net packet builder ─────────────────────────────────────────
def build_artdmx(universe: int, data: list) -> bytes:
    global seq_num
    seq_num = (seq_num % 255) + 1   # 1-255, never 0 (0 = sequencing disabled per spec)
    length  = len(data)
    if length % 2: length += 1
    payload = bytes(data[:length]) + bytes(max(0, length - len(data)))
    hdr = (
        b"Art-Net\x00"
        + struct.pack("<H", 0x5000)
        + struct.pack(">H", 14)
        + bytes([seq_num, 0])
        + struct.pack("<H", universe & 0x7FFF)
        + struct.pack(">H", length)
    )
    return hdr + payload

# ── Animations ────────────────────────────────────────────────────
def hue_to_rgb(hue_deg: float):
    h = (hue_deg % 360) / 60
    i = int(h)
    f = h - i
    vals = [
        (255, int(255*f), 0),
        (int(255*(1-f)), 255, 0),
        (0, 255, int(255*f)),
        (0, int(255*(1-f)), 255),
        (int(255*f), 0, 255),
        (255, 0, int(255*(1-f))),
    ]
    return vals[i % 6]

def demo_tick(t: float):
    r, g, b = hue_to_rgb(t * 60)
    pan  = int(127 + 127 * math.sin(t * 0.5))
    tilt = int(127 + 100 * math.sin(t * 0.3 + 1.0))
    with lock:
        dmx[base()+CH_MASTER] = 255
        dmx[base()+CH_RED]    = r
        dmx[base()+CH_GREEN]  = g
        dmx[base()+CH_BLUE]   = b
        dmx[base()+CH_WHITE]  = 0
        dmx[base()+CH_PAN]    = pan
        dmx[base()+CH_TILT]   = tilt

def rainbow_tick(dt: float):
    global rainbow_hue
    rainbow_hue = (rainbow_hue + dt * 120) % 360   # full cycle ~3s
    r, g, b = hue_to_rgb(rainbow_hue)
    with lock:
        dmx[base()+CH_MASTER] = 255
        dmx[base()+CH_RED]    = r
        dmx[base()+CH_GREEN]  = g
        dmx[base()+CH_BLUE]   = b
        dmx[base()+CH_WHITE]  = 0

# ── Named commands ────────────────────────────────────────────────
def cmd_blackout():
    global demo_mode, rainbow_mode
    demo_mode = rainbow_mode = False
    with lock:
        for i in range(DMX_FP): dmx[base() + i] = 0

def cmd_full():
    global demo_mode, rainbow_mode
    demo_mode = rainbow_mode = False
    with lock:
        dmx[base()+CH_MASTER] = 255
        dmx[base()+CH_RED]    = 0
        dmx[base()+CH_GREEN]  = 0
        dmx[base()+CH_BLUE]   = 0
        dmx[base()+CH_WHITE]  = 255

def cmd_highlight():
    global demo_mode, rainbow_mode
    demo_mode = rainbow_mode = False
    with lock:
        dmx[base()+CH_MASTER] = 255
        dmx[base()+CH_RED]    = 255
        dmx[base()+CH_GREEN]  = 255
        dmx[base()+CH_BLUE]   = 255
        dmx[base()+CH_WHITE]  = 255

def cmd_center():
    with lock:
        dmx[base()+CH_PAN]  = 128
        dmx[base()+CH_TILT] = 128

# ── Sender thread ─────────────────────────────────────────────────
HEARTBEAT_INTERVAL = 1.0   # send keep-alive every 1s even if nothing changed

def sender():
    global pkt_count, demo_t, dmx_last_sent
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Bind to the correct outgoing interface.  On macOS with VPN or Docker
    # the unbound socket may route UDP through a tunnel — Wireshark then shows
    # nothing on the LAN even though sendto() returns no error.
    bind_ip = BIND_IP or get_local_ip_for(TARGET_IP)
    if bind_ip:
        try:
            sock.bind((bind_ip, 0))
        except Exception as e:
            pass   # non-fatal — OS picks interface

    tick     = 1.0 / SEND_RATE   # how often we CHECK for changes (40Hz)
    prev_t   = time.time()
    last_sent_t = 0.0            # timestamp of last transmitted packet

    while running:
        t0 = time.time()
        dt = t0 - prev_t
        prev_t = t0

        # Advance animations
        if demo_mode:
            demo_t += dt
            demo_tick(demo_t)
        elif rainbow_mode:
            rainbow_tick(dt)

        with lock:
            payload = list(dmx[:512])

        changed   = (payload != dmx_last_sent)
        heartbeat = (t0 - last_sent_t >= HEARTBEAT_INTERVAL)

        if changed or heartbeat:
            global last_send_reason
            last_send_reason = "change" if changed else "heartbeat"
            uni = target["universe"]
            pkt = build_artdmx(uni, payload)
            try:
                sock.sendto(pkt, (target["ip"], ARTNET_PORT))
                pkt_count  += 1
                dmx_last_sent = payload
                last_sent_t   = t0
            except Exception as e:
                pass   # sendto rarely fails on UDP; errors show as pkt_count stalling

        elapsed = time.time() - t0
        time.sleep(max(0, tick - elapsed))
    sock.close()

# ── Flask app ─────────────────────────────────────────────────────
app = Flask(__name__)

HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Art-Net Test</title>
<style>
  :root{
    --bg:#0a0a0f;--surface:#13131a;--surface2:#1c1c26;
    --border:#2a2a3a;--accent:#00e5ff;--accent2:#ff6b35;
    --accent3:#a855f7;--text:#e0e0f0;--text-dim:#6b6b8a;
    --success:#22c55e;--danger:#ef4444;
    --mono:'Share Tech Mono',monospace;--sans:'Barlow',sans-serif;
  }
  *{margin:0;padding:0;box-sizing:border-box;}
  body{background:var(--bg);color:var(--text);font-family:var(--sans);
       min-height:100vh;overflow-x:hidden;}
  body::before{content:'';position:fixed;inset:0;background:repeating-linear-gradient(
    0deg,transparent,transparent 2px,rgba(0,229,255,0.012) 2px,rgba(0,229,255,0.012) 4px);
    pointer-events:none;z-index:1000;}

  header{display:flex;align-items:center;justify-content:space-between;
    padding:10px 16px;border-bottom:1px solid var(--border);background:var(--surface);
    position:sticky;top:0;z-index:100;gap:12px;flex-wrap:wrap;}
  .logo{font-family:var(--mono);font-size:15px;color:var(--accent);
    letter-spacing:2px;text-transform:uppercase;white-space:nowrap;}
  .logo span{color:var(--text-dim);}
  .pkt-counter{font-family:var(--mono);font-size:11px;color:var(--text-dim);}
  .pkt-counter b{color:var(--success);}

  /* Target bar */
  .target-bar{display:flex;align-items:center;gap:8px;flex-wrap:wrap;
    padding:10px 16px;background:var(--surface);border-bottom:1px solid var(--border);}
  .tl{font-family:var(--mono);font-size:10px;color:var(--text-dim);white-space:nowrap;}
  .ti{background:var(--surface2);border:1px solid var(--border);color:var(--text);
    padding:4px 8px;font-family:var(--mono);font-size:12px;border-radius:3px;outline:none;}
  .ti:focus{border-color:var(--accent);}
  .ti-ip{width:140px;}
  .ti-num{width:64px;text-align:center;}

  /* Main grid */
  .main{display:grid;grid-template-columns:1fr;gap:1px;background:var(--border);}
  @media(min-width:700px){
    .main{grid-template-columns:1fr 1fr;}
  }
  .panel{background:var(--bg);padding:16px;}
  .panel-title{font-family:var(--mono);font-size:11px;color:var(--accent);
    letter-spacing:3px;text-transform:uppercase;margin-bottom:14px;
    padding-bottom:8px;border-bottom:1px solid var(--border);}

  /* Faders */
  .faders-row{display:grid;grid-template-columns:repeat(5,1fr);gap:8px;align-items:end;}
  .fader-group{display:flex;flex-direction:column;align-items:center;gap:6px;}
  .fader-label{font-family:var(--mono);font-size:10px;color:var(--text-dim);letter-spacing:1px;}
  .vslider-wrap{width:36px;height:140px;display:flex;align-items:center;justify-content:center;}
  input[type=range].vertical{writing-mode:vertical-lr;direction:rtl;width:36px;height:140px;
    -webkit-appearance:slider-vertical;appearance:slider-vertical;cursor:pointer;}
  input[type=range]{-webkit-appearance:none;appearance:none;background:transparent;cursor:pointer;}
  input[type=range].vertical::-webkit-slider-thumb{-webkit-appearance:none;width:32px;height:14px;
    border-radius:3px;background:var(--accent);box-shadow:0 0 10px var(--accent);border:2px solid var(--bg);}
  .fader-m  input[type=range]::-webkit-slider-thumb{background:#ffdd44;box-shadow:0 0 10px #ffdd44;}
  .fader-r  input[type=range]::-webkit-slider-thumb{background:#ff4444;box-shadow:0 0 10px #ff4444;}
  .fader-g  input[type=range]::-webkit-slider-thumb{background:#44ff44;box-shadow:0 0 10px #44ff44;}
  .fader-b  input[type=range]::-webkit-slider-thumb{background:#4488ff;box-shadow:0 0 10px #4488ff;}
  .fader-w  input[type=range]::-webkit-slider-thumb{background:#ffffff;box-shadow:0 0 10px #aaa;}
  .num-input{width:44px;background:var(--surface2);border:1px solid var(--border);
    color:var(--text);padding:4px 2px;font-family:var(--mono);font-size:13px;
    border-radius:4px;outline:none;text-align:center;}
  .preview-row{display:flex;justify-content:center;margin-top:14px;}
  .led-preview{width:64px;height:64px;border-radius:50%;border:2px solid var(--border);
    transition:background 0.08s,box-shadow 0.08s;background:#000;}

  /* Motion */
  .motion-row{display:grid;grid-template-columns:44px 1fr 52px;
    align-items:center;gap:10px;margin-bottom:12px;}
  .motion-label{font-family:var(--mono);font-size:11px;color:var(--text-dim);letter-spacing:1px;}
  input[type=range].horizontal{-webkit-appearance:none;appearance:none;width:100%;height:6px;
    background:var(--surface2);border-radius:3px;outline:none;cursor:pointer;}
  input[type=range].horizontal::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;
    border-radius:50%;background:var(--accent2);box-shadow:0 0 10px var(--accent2);border:2px solid var(--bg);}

  /* Buttons panel */
  .btn-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;}
  @media(min-width:400px){.btn-grid{grid-template-columns:repeat(3,1fr);}}
  .btn{padding:14px 8px;border:1px solid var(--border);background:var(--surface2);
    color:var(--text);font-family:var(--mono);font-size:12px;cursor:pointer;
    border-radius:4px;letter-spacing:1px;transition:all 0.15s;text-transform:uppercase;
    touch-action:manipulation;width:100%;}
  .btn:active{opacity:0.7;}
  .btn.primary{border-color:var(--accent);color:var(--accent);background:rgba(0,229,255,0.08);}
  .btn.danger {border-color:var(--danger);color:var(--danger);}
  .btn.success{border-color:var(--success);color:var(--success);}
  .btn.active {background:rgba(168,85,247,0.22);border-color:var(--accent3);color:var(--accent3);}
  .btn.rainbow-btn{background:linear-gradient(90deg,
    #ff000033,#ffff0033,#00ff0033,#00ffff33,#0000ff33,#ff00ff33);
    border-color:var(--border);}
  .btn.rainbow-btn.active{background:linear-gradient(90deg,
    #ff0000aa,#ffff00aa,#00ff00aa,#00ffffaa,#0000ffaa,#ff00ffaa);
    border-color:transparent;color:#fff;animation:rainbow-shift 2s linear infinite;}
  @keyframes rainbow-shift{0%{filter:hue-rotate(0deg)}100%{filter:hue-rotate(360deg)}}

  /* Info bar */
  .info-bar{display:flex;gap:16px;flex-wrap:wrap;font-family:var(--mono);font-size:11px;
    color:var(--text-dim);}
  .info-bar span b{color:var(--accent);}

  /* Toast */
  #toast{position:fixed;bottom:16px;right:16px;left:16px;background:var(--surface2);
    border:1px solid var(--border);color:var(--text);padding:10px 18px;
    font-family:var(--mono);font-size:12px;border-radius:4px;opacity:0;
    transition:opacity 0.2s;z-index:999;pointer-events:none;text-align:center;}
  #toast.show{opacity:1;}
  #toast.ok {border-color:var(--success);color:var(--success);}
  #toast.err{border-color:var(--danger);color:var(--danger);}
  @media(min-width:700px){#toast{left:auto;width:auto;}}
</style>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow:wght@300;400;600&display=swap" rel="stylesheet">
</head>
<body>

<header>
  <div class="logo">ART<span>-</span>NET <span style="font-size:10px">TEST</span></div>
  <div class="info-bar" id="infoBar">
    <span>PKT <b id="pktCount">0</b></span>
    <span id="sendReason" style="color:var(--text-dim)">—</span>
    <span id="modeLabel" style="color:var(--text-dim)">MANUAL</span>
  </div>
</header>

<!-- Target config bar -->
<div class="target-bar">
  <span class="tl">IP</span>
  <input class="ti ti-ip" id="cfgIp" value="255.255.255.255" placeholder="Target IP"
    onkeydown="if(event.key==='Enter')applyTarget()">
  <span class="tl">UNI</span>
  <input class="ti ti-num" id="cfgUni" type="number" value="0" min="0" max="32767"
    onkeydown="if(event.key==='Enter')applyTarget()">
  <span class="tl">ADDR</span>
  <input class="ti ti-num" id="cfgAddr" type="number" value="1" min="1" max="512"
    onkeydown="if(event.key==='Enter')applyTarget()">
  <button class="btn primary" style="padding:5px 14px;font-size:11px;" onclick="applyTarget()">SET</button>
</div>

<div class="main">

  <!-- RGBW + Master faders -->
  <div class="panel">
    <div class="panel-title">// Channels</div>
    <div class="faders-row">
      <div class="fader-group fader-m">
        <div class="fader-label">MST</div>
        <div class="vslider-wrap">
          <input type="range" class="vertical" min="0" max="255" value="255"
            id="fM" oninput="onFader('M',+this.value)">
        </div>
        <input type="number" class="num-input" min="0" max="255" value="255" id="vM"
          oninput="syncFader('M',+this.value)">
      </div>
      <div class="fader-group fader-r">
        <div class="fader-label">RED</div>
        <div class="vslider-wrap">
          <input type="range" class="vertical" min="0" max="255" value="0"
            id="fR" oninput="onFader('R',+this.value)">
        </div>
        <input type="number" class="num-input" min="0" max="255" value="0" id="vR"
          oninput="syncFader('R',+this.value)">
      </div>
      <div class="fader-group fader-g">
        <div class="fader-label">GRN</div>
        <div class="vslider-wrap">
          <input type="range" class="vertical" min="0" max="255" value="0"
            id="fG" oninput="onFader('G',+this.value)">
        </div>
        <input type="number" class="num-input" min="0" max="255" value="0" id="vG"
          oninput="syncFader('G',+this.value)">
      </div>
      <div class="fader-group fader-b">
        <div class="fader-label">BLU</div>
        <div class="vslider-wrap">
          <input type="range" class="vertical" min="0" max="255" value="0"
            id="fB" oninput="onFader('B',+this.value)">
        </div>
        <input type="number" class="num-input" min="0" max="255" value="0" id="vB"
          oninput="syncFader('B',+this.value)">
      </div>
      <div class="fader-group fader-w">
        <div class="fader-label">WHT</div>
        <div class="vslider-wrap">
          <input type="range" class="vertical" min="0" max="255" value="0"
            id="fW" oninput="onFader('W',+this.value)">
        </div>
        <input type="number" class="num-input" min="0" max="255" value="0" id="vW"
          oninput="syncFader('W',+this.value)">
      </div>
    </div>
    <div class="preview-row">
      <div class="led-preview" id="ledPreview"></div>
    </div>
  </div>

  <!-- Motion -->
  <div class="panel">
    <div class="panel-title">// Motion</div>
    <div class="motion-row">
      <div class="motion-label">PAN</div>
      <input type="range" class="horizontal" min="0" max="255" value="128"
        id="fPan" oninput="onMotion('Pan',+this.value)">
      <input type="number" class="num-input" min="0" max="255" value="128" id="vPan"
        style="color:var(--accent2);" oninput="syncMotion('Pan',+this.value)">
    </div>
    <div class="motion-row">
      <div class="motion-label">TILT</div>
      <input type="range" class="horizontal" min="0" max="255" value="128"
        id="fTilt" oninput="onMotion('Tilt',+this.value)">
      <input type="number" class="num-input" min="0" max="255" value="128" id="vTilt"
        style="color:var(--accent2);" oninput="syncMotion('Tilt',+this.value)">
    </div>
    <div style="margin-top:10px;font-family:var(--mono);font-size:10px;color:var(--text-dim);" id="panTiltDeg">
      Pan: 90°  Tilt: 90°
    </div>
  </div>

  <!-- Quick actions -->
  <div class="panel">
    <div class="panel-title">// Quick Actions</div>
    <div class="btn-grid">
      <button class="btn danger"   onclick="doCmd('blackout')">⬛ BLACKOUT</button>
      <button class="btn success"  onclick="doCmd('full')">☀ FULL</button>
      <button class="btn primary"  id="hlBtn"
        onmousedown="hlPress()" ontouchstart="hlPress(event)"
        onmouseup="hlRelease()" ontouchend="hlRelease(event)"
        onmouseleave="hlRelease()">✦ HIGHLIGHT</button>
      <button class="btn rainbow-btn" id="rainbowBtn" onclick="toggleRainbow()">◈ RAINBOW</button>
      <button class="btn"          onclick="doCmd('center')">⊕ CENTER</button>
      <button class="btn"          id="demoBtn" onclick="toggleDemo()">▶ DEMO</button>
    </div>
  </div>

  <!-- Live values -->
  <div class="panel">
    <div class="panel-title">// Live DMX</div>
    <div id="liveTable" style="font-family:var(--mono);font-size:12px;line-height:2;"></div>
  </div>

</div>
<div id="toast"></div>

<script>
var sendTimer=null, pollTimer=null, rainbowActive=false, demoActive=false;

function toast(msg,type){
  var el=document.getElementById('toast');
  el.textContent=msg; el.className='show '+(type||'ok');
  clearTimeout(el._t); el._t=setTimeout(function(){el.className='';},1800);
}

// ── Target config ─────────────────────────────────────────────────
function applyTarget(){
  var ip=document.getElementById('cfgIp').value.trim();
  var uni=+document.getElementById('cfgUni').value;
  var addr=+document.getElementById('cfgAddr').value;
  fetch('/api/target',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ip:ip,universe:uni,startAddr:addr})
  }).then(function(r){return r.json();}).then(function(d){
    toast('Target → '+ip+' uni:'+uni+' addr:'+addr);
  });
}

// ── Fader sync ────────────────────────────────────────────────────
var faderMap={M:'fM',R:'fR',G:'fG',B:'fB',W:'fW'};
var numMap  ={M:'vM',R:'vR',G:'vG',B:'vB',W:'vW'};
function onFader(ch,v){
  document.getElementById(numMap[ch]).value=v;
  updatePreview(); debounceSend();
}
function syncFader(ch,v){
  document.getElementById(faderMap[ch]).value=v;
  updatePreview(); debounceSend();
}
function onMotion(axis,v){
  document.getElementById('v'+axis).value=v;
  updatePanTiltLabel(); debounceSend();
}
function syncMotion(axis,v){
  document.getElementById('f'+axis).value=v;
  updatePanTiltLabel(); debounceSend();
}

function getVals(){
  return{
    master:+document.getElementById('vM').value,
    r:+document.getElementById('vR').value,
    g:+document.getElementById('vG').value,
    b:+document.getElementById('vB').value,
    w:+document.getElementById('vW').value,
    pan:+document.getElementById('vPan').value,
    tilt:+document.getElementById('vTilt').value
  };
}

function updatePreview(){
  var v=getVals();
  var rp=Math.min(255,Math.round(v.r*v.master/255)+Math.round(v.w*v.master/255));
  var gp=Math.min(255,Math.round(v.g*v.master/255)+Math.round(v.w*v.master/255));
  var bp=Math.min(255,Math.round(v.b*v.master/255)+Math.round(v.w*v.master/255));
  var p=document.getElementById('ledPreview');
  var hex='rgb('+rp+','+gp+','+bp+')';
  var br=(rp+gp+bp)/3;
  p.style.background=hex;
  p.style.boxShadow=br>8?'0 0 '+(18+br/4)+'px '+(6+br/8)+'px '+hex:'none';
}

function updatePanTiltLabel(){
  var pan =Math.round(+document.getElementById('vPan').value /255*270);
  var tilt=Math.round(+document.getElementById('vTilt').value/255*270);
  document.getElementById('panTiltDeg').textContent='Pan: '+pan+'°  Tilt: '+tilt+'°';
}

function debounceSend(){
  clearTimeout(sendTimer);
  sendTimer=setTimeout(sendValues,30);
}

function sendValues(){
  var v=getVals();
  fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(v)});
}

// ── Highlight hold button ─────────────────────────────────────────
var _hlActive=false, _hlSnapshot=null;
function hlPress(e){
  if(e) e.preventDefault();
  if(_hlActive) return;
  _hlActive=true;
  _hlSnapshot=getVals();
  document.getElementById('hlBtn').classList.add('active');
  fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({cmd:'highlight'})});
}
function hlRelease(e){
  if(e) e.preventDefault();
  if(!_hlActive) return;
  _hlActive=false;
  document.getElementById('hlBtn').classList.remove('active');
  if(_hlSnapshot){
    fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify(_hlSnapshot)});
    _hlSnapshot=null;
  }
}

// ── Commands ─────────────────────────────────────────────────────
function doCmd(cmd){
  if(cmd==='blackout'||cmd==='full'||cmd==='highlight'){
    rainbowActive=false; demoActive=false;
    document.getElementById('rainbowBtn').classList.remove('active');
    document.getElementById('demoBtn').classList.remove('active');
  }
  fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({cmd:cmd})
  }).then(function(){
    toast(cmd.toUpperCase());
    setTimeout(syncFromServer,100);
  });
}

function toggleRainbow(){
  rainbowActive=!rainbowActive;
  if(rainbowActive) demoActive=false;
  document.getElementById('rainbowBtn').classList.toggle('active',rainbowActive);
  document.getElementById('demoBtn').classList.remove('active');
  fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({cmd:rainbowActive?'rainbow_on':'rainbow_off'})});
  toast(rainbowActive?'RAINBOW ON':'RAINBOW OFF');
}

function toggleDemo(){
  demoActive=!demoActive;
  if(demoActive) rainbowActive=false;
  document.getElementById('demoBtn').classList.toggle('active',demoActive);
  document.getElementById('rainbowBtn').classList.remove('active');
  fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({cmd:demoActive?'demo_on':'demo_off'})});
  toast(demoActive?'DEMO ON':'DEMO OFF');
}

// ── State poll ────────────────────────────────────────────────────
function syncFromServer(){
  fetch('/api/state').then(function(r){return r.json();}).then(function(d){
    // Sync faders
    var map={M:d.master,R:d.r,G:d.g,B:d.b,W:d.w};
    for(var k in map){
      document.getElementById(faderMap[k]).value=map[k];
      document.getElementById(numMap[k]).value=map[k];
    }
    document.getElementById('fPan').value=d.pan;
    document.getElementById('vPan').value=d.pan;
    document.getElementById('fTilt').value=d.tilt;
    document.getElementById('vTilt').value=d.tilt;
    // Mode buttons
    rainbowActive=d.rainbow_mode;
    demoActive=d.demo_mode;
    document.getElementById('rainbowBtn').classList.toggle('active',rainbowActive);
    document.getElementById('demoBtn').classList.toggle('active',demoActive);
    // Mode label
    var lbl=document.getElementById('modeLabel');
    lbl.textContent=d.demo_mode?'DEMO':d.rainbow_mode?'RAINBOW':'MANUAL';
    lbl.style.color=d.demo_mode?'var(--success)':d.rainbow_mode?'var(--accent3)':'var(--text-dim)';
    // Target bar — only update when user isn't actively editing the fields
    var focusId=document.activeElement?document.activeElement.id:'';
    var cfgFocused=(focusId==='cfgIp'||focusId==='cfgUni'||focusId==='cfgAddr');
    if(!cfgFocused){
      document.getElementById('cfgIp').value=d.target.ip;
      document.getElementById('cfgUni').value=d.target.universe;
      document.getElementById('cfgAddr').value=d.target.startAddr;
    }
    // Pkt counter
    document.getElementById('pktCount').textContent=d.pkt_count;
    var sr=document.getElementById('sendReason');
    sr.textContent=d.last_send_reason;
    sr.style.color=d.last_send_reason==='change'?'var(--success)':'var(--text-dim)';
    updatePreview();
    updatePanTiltLabel();
    // Live table
    var rows='';
    var chLabels=['MST','RED','GRN','BLU','WHT','PAN','TLT'];
    var chColors=['#ffdd44','#ff4444','#44ff44','#4488ff','#ffffff','#ff6b35','#ff6b35'];
    var vals=[d.master,d.r,d.g,d.b,d.w,d.pan,d.tilt];
    for(var i=0;i<7;i++){
      var addr=d.target.startAddr+i;
      var pct=Math.round(vals[i]/255*100);
      var bar='';
      var filled=Math.round(vals[i]/255*16);
      for(var j=0;j<16;j++) bar+=j<filled?'█':'░';
      rows+='<div style="display:flex;align-items:center;gap:8px;margin-bottom:2px;">'
        +'<span style="color:var(--text-dim);width:32px;">'+addr+'</span>'
        +'<span style="color:'+chColors[i]+';width:28px;">'+chLabels[i]+'</span>'
        +'<span style="width:28px;text-align:right;">'+vals[i]+'</span>'
        +'<span style="color:'+chColors[i]+';font-size:10px;opacity:0.7;">'+bar+'</span>'
        +'<span style="color:var(--text-dim);font-size:10px;width:32px;">'+pct+'%</span>'
        +'</div>';
    }
    document.getElementById('liveTable').innerHTML=rows;
  });
}

// Init + poll
syncFromServer();
pollTimer=setInterval(syncFromServer,250);
updatePreview();
updatePanTiltLabel();
</script>
</body>
</html>
"""

@app.route("/")
def serve_ui():
    return HTML

@app.route("/api/state")
def api_state():
    b = base()
    with lock:
        return jsonify({
            "master":       dmx[b + CH_MASTER],
            "r":            dmx[b + CH_RED],
            "g":            dmx[b + CH_GREEN],
            "b":            dmx[b + CH_BLUE],
            "w":            dmx[b + CH_WHITE],
            "pan":          dmx[b + CH_PAN],
            "tilt":         dmx[b + CH_TILT],
            "demo_mode":       demo_mode,
            "rainbow_mode":    rainbow_mode,
            "pkt_count":       pkt_count,
            "last_send_reason": last_send_reason,
            "target":          dict(target),
        })

@app.route("/api/set", methods=["POST"])
def api_set():
    global demo_mode, rainbow_mode
    data = request.get_json() or {}
    demo_mode = rainbow_mode = False
    b = base()
    with lock:
        if "master" in data: dmx[b + CH_MASTER] = clamp(data["master"])
        if "r"      in data: dmx[b + CH_RED]    = clamp(data["r"])
        if "g"      in data: dmx[b + CH_GREEN]   = clamp(data["g"])
        if "b"      in data: dmx[b + CH_BLUE]    = clamp(data["b"])
        if "w"      in data: dmx[b + CH_WHITE]   = clamp(data["w"])
        if "pan"    in data: dmx[b + CH_PAN]     = clamp(data["pan"])
        if "tilt"   in data: dmx[b + CH_TILT]    = clamp(data["tilt"])
    return jsonify({"status": "ok"})

@app.route("/api/command", methods=["POST"])
def api_command():
    global demo_mode, rainbow_mode
    data = request.get_json() or {}
    cmd  = data.get("cmd", "")
    if   cmd == "blackout":   cmd_blackout()
    elif cmd == "full":       cmd_full()
    elif cmd == "highlight":  cmd_highlight()
    elif cmd == "center":     cmd_center()
    elif cmd == "rainbow_on":  demo_mode = False;  rainbow_mode = True
    elif cmd == "rainbow_off": rainbow_mode = False
    elif cmd == "demo_on":     rainbow_mode = False; demo_mode = True
    elif cmd == "demo_off":    demo_mode = False
    return jsonify({"status": "ok"})

@app.route("/api/target", methods=["POST"])
def api_target():
    data = request.get_json() or {}
    if "ip"        in data: target["ip"]        = str(data["ip"])
    if "universe"  in data: target["universe"]  = int(data["universe"])
    if "startAddr" in data: target["startAddr"] = max(1, min(512, int(data["startAddr"])))
    return jsonify({"status": "ok", "target": dict(target)})

# ── ANSI helpers ──────────────────────────────────────────────────
RESET  = "\033[0m"; BOLD = "\033[1m"; DIM = "\033[2m"
GREEN  = "\033[92m"; CYAN = "\033[96m"; YELLOW = "\033[93m"
RED    = "\033[91m"; MAGENTA = "\033[95m"; WHITE = "\033[97m"; GRAY = "\033[90m"

def bar(val, width=20, color=GREEN):
    filled = int(val / 255 * width)
    return color + "█" * filled + GRAY + "░" * (width - filled) + RESET

def render():
    b = base()
    with lock:
        d = list(dmx[:512])
    master = d[b + CH_MASTER]; r = d[b + CH_RED]; g = d[b + CH_GREEN]
    bv = d[b + CH_BLUE]; w = d[b + CH_WHITE]; pan = d[b + CH_PAN]; tilt = d[b + CH_TILT]
    r_o = r * master // 255; g_o = g * master // 255
    b_o = bv * master // 255; w_o = w * master // 255
    mode = (f"{GREEN}DEMO{RESET}" if demo_mode else
            f"{MAGENTA}RAINBOW{RESET}" if rainbow_mode else
            f"{YELLOW}MANUAL{RESET}")
    reason_col = GREEN if last_send_reason == "change" else GRAY
    reason_str = reason_col + last_send_reason + RESET
    lines = [
        f"{BOLD}{CYAN}╔═══════════════════════════════════════════════╗{RESET}",
        f"{BOLD}{CYAN}║  Art-Net Test  [{WHITE}http://localhost:{UI_PORT}{CYAN}]       ║{RESET}",
        f"{BOLD}{CYAN}╚═══════════════════════════════════════════════╝{RESET}",
        f"",
        f"  {target['ip']}:{ARTNET_PORT}  uni:{target['universe']}  addr:{target['startAddr']}  "
        f"pkts:{BOLD}{WHITE}{pkt_count}{RESET}  last:{reason_str}  {mode}",
        f"",
        f"  {YELLOW}Ch{b+1:3d}{RESET}  MASTER  {master:3d}  {bar(master, 22, YELLOW)}",
        f"  {RED}Ch{b+2:3d}{RESET}  RED     {r:3d}  {bar(r, 22, RED)}  →{r_o}",
        f"  {GREEN}Ch{b+3:3d}{RESET}  GREEN   {g:3d}  {bar(g, 22, GREEN)}  →{g_o}",
        f"  {CYAN}Ch{b+4:3d}{RESET}  BLUE    {bv:3d}  {bar(bv, 22, CYAN)}  →{b_o}",
        f"  {WHITE}Ch{b+5:3d}{RESET}  WHITE   {w:3d}  {bar(w, 22, WHITE)}  →{w_o}",
        f"  {MAGENTA}Ch{b+6:3d}{RESET}  PAN     {pan:3d}  {bar(pan, 22, MAGENTA)}  {pan/255*270:.0f}°",
        f"  {MAGENTA}Ch{b+7:3d}{RESET}  TILT    {tilt:3d}  {bar(tilt, 22, MAGENTA)}  {tilt/255*270:.0f}°",
        f"",
        f"  {GRAY}[q]quit [SPACE]demo [0]blackout [f]full [h]highlight [n]rainbow [s]snap{RESET}",
    ]
    return "\n".join(lines)

def display():
    while running:
        frame = render()
        # \033[H → cursor to top-left   \033[J → clear to end of screen
        # Avoids the horizontal-drift bug caused by line-count-based cursor-up.
        sys.stdout.write("\033[H\033[J" + frame + "\n")
        sys.stdout.flush()
        time.sleep(1.0 / 15)

def getch():
    fd = sys.stdin.fileno(); old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd); return sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)

def input_loop():
    global demo_mode, rainbow_mode, running
    while running:
        ch = getch()
        b = base()
        with lock:
            if   ch == 'q': running = False
            elif ch == ' ':
                demo_mode = not demo_mode
                if demo_mode: rainbow_mode = False
            elif ch == 'n':
                rainbow_mode = not rainbow_mode
                if rainbow_mode: demo_mode = False
            elif ch == '0': cmd_blackout()
            elif ch == 'f': cmd_full()
            elif ch == 'h': cmd_highlight()
            elif ch == 'c': cmd_center()
            elif ch == 'm': dmx[b+CH_MASTER] = clamp(dmx[b+CH_MASTER]+10)
            elif ch == 'M': dmx[b+CH_MASTER] = clamp(dmx[b+CH_MASTER]-10)
            elif ch == 'r': dmx[b+CH_RED]    = clamp(dmx[b+CH_RED]+10)
            elif ch == 'R': dmx[b+CH_RED]    = clamp(dmx[b+CH_RED]-10)
            elif ch == 'g': dmx[b+CH_GREEN]  = clamp(dmx[b+CH_GREEN]+10)
            elif ch == 'G': dmx[b+CH_GREEN]  = clamp(dmx[b+CH_GREEN]-10)
            elif ch == 'b': dmx[b+CH_BLUE]   = clamp(dmx[b+CH_BLUE]+10)
            elif ch == 'B': dmx[b+CH_BLUE]   = clamp(dmx[b+CH_BLUE]-10)
            elif ch == 'w': dmx[b+CH_WHITE]  = clamp(dmx[b+CH_WHITE]+10)
            elif ch == 'W': dmx[b+CH_WHITE]  = clamp(dmx[b+CH_WHITE]-10)
            elif ch == 'p': dmx[b+CH_PAN]    = clamp(dmx[b+CH_PAN]+10)
            elif ch == 'P': dmx[b+CH_PAN]    = clamp(dmx[b+CH_PAN]-10)
            elif ch == 't': dmx[b+CH_TILT]   = clamp(dmx[b+CH_TILT]+10)
            elif ch == 'T': dmx[b+CH_TILT]   = clamp(dmx[b+CH_TILT]-10)
            elif ch == 's':
                snap = (f"M={dmx[b+CH_MASTER]} R={dmx[b+CH_RED]} G={dmx[b+CH_GREEN]} "
                        f"B={dmx[b+CH_BLUE]} W={dmx[b+CH_WHITE]} "
                        f"PAN={dmx[b+CH_PAN]} TILT={dmx[b+CH_TILT]}")
                snapshots.append(snap)

# ── Entry point ───────────────────────────────────────────────────
if __name__ == "__main__":
    sys.stdout.write("\033[2J\033[H"); sys.stdout.flush()  # clear screen once at start

    threading.Thread(target=sender,     daemon=True).start()
    threading.Thread(target=display,    daemon=True).start()
    threading.Thread(target=input_loop, daemon=True).start()

    # Flask in a daemon thread so KeyboardInterrupt still works
    flask_thread = threading.Thread(
        target=lambda: app.run(host="0.0.0.0", port=UI_PORT,
                               debug=False, use_reloader=False),
        daemon=True
    )
    flask_thread.start()

    try:
        while running:
            time.sleep(0.1)
    except KeyboardInterrupt:
        running = False

    # Blackout before exit
    with lock:
        for i in range(DMX_FP): dmx[START_ADDR - 1 + i] = 0
    time.sleep(0.1)
    print("\n\033[?25h\033[0m")
