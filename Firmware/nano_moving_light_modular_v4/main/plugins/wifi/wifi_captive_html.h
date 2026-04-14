#pragma once

// ── Captive Portal HTML ───────────────────────────────────────────
// Served at GET / when nodeRole == ROLE_AP_PORTAL.
// User enters SSID + password → POST /wifi/save → NVS → reboot.
// ─────────────────────────────────────────────────────────────────

const char CAPTIVE_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>MiniHead WiFi Setup</title>
<style>
  :root{--bg:#0a0a0f;--surface:#13131a;--surface2:#1c1c26;--border:#2a2a3a;--accent:#00e5ff;--text:#e0e0f0;--text-dim:#6b6b8a;--success:#22c55e;--danger:#ef4444;--mono:'Courier New',monospace;}
  *{margin:0;padding:0;box-sizing:border-box;}
  body{background:var(--bg);color:var(--text);font-family:'Helvetica Neue',sans-serif;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px;}
  .card{background:var(--surface);border:1px solid var(--border);border-radius:8px;padding:28px 24px;width:100%;max-width:360px;}
  .logo{font-family:var(--mono);font-size:18px;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:6px;}
  .subtitle{font-family:var(--mono);font-size:11px;color:var(--text-dim);margin-bottom:24px;letter-spacing:1px;}
  .field{margin-bottom:16px;}
  label{display:block;font-family:var(--mono);font-size:10px;color:var(--text-dim);letter-spacing:2px;text-transform:uppercase;margin-bottom:6px;}
  input[type=text],input[type=password]{width:100%;background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:10px 12px;font-family:var(--mono);font-size:13px;border-radius:4px;outline:none;transition:border-color 0.15s;}
  input:focus{border-color:var(--accent);}
  .scan-list{margin-bottom:16px;}
  .scan-item{padding:8px 10px;background:var(--surface2);border:1px solid var(--border);border-radius:3px;margin-bottom:4px;cursor:pointer;font-family:var(--mono);font-size:12px;display:flex;align-items:center;justify-content:space-between;transition:border-color 0.1s;}
  .scan-item:hover{border-color:var(--accent);}
  .scan-rssi{color:var(--text-dim);font-size:10px;}
  .scan-lock{color:var(--text-dim);font-size:10px;margin-left:4px;}
  .btn{width:100%;padding:12px;background:rgba(0,229,255,0.08);border:1px solid var(--accent);color:var(--accent);font-family:var(--mono);font-size:13px;letter-spacing:2px;text-transform:uppercase;border-radius:4px;cursor:pointer;transition:all 0.15s;margin-top:4px;}
  .btn:active{opacity:0.7;}
  .hint{font-family:var(--mono);font-size:10px;color:var(--text-dim);margin-top:18px;text-align:center;line-height:1.6;}
  .saving{display:none;text-align:center;padding:20px 0;font-family:var(--mono);font-size:12px;color:var(--success);}
</style>
</head>
<body>
<div class="card">
  <div class="logo">MiniHead</div>
  <div class="subtitle">// WiFi Setup</div>

  <div id="scan-section">
    <div style="font-family:var(--mono);font-size:10px;color:var(--text-dim);letter-spacing:2px;text-transform:uppercase;margin-bottom:8px;">Available Networks</div>
    <div class="scan-list" id="scanList">
      <div style="font-family:var(--mono);font-size:11px;color:var(--text-dim);padding:8px 0;">Scanning...</div>
    </div>
  </div>

  <form id="wifiForm" onsubmit="submitForm(event)">
    <div class="field">
      <label>Network (SSID)</label>
      <input type="text" id="ssid" name="ssid" placeholder="Enter SSID..." autocomplete="off" required>
    </div>
    <div class="field">
      <label>Password</label>
      <input type="password" id="pass" name="pass" placeholder="Enter password..." autocomplete="off">
    </div>
    <button class="btn" type="submit">CONNECT &amp; SAVE</button>
  </form>

  <div class="saving" id="savingMsg">
    &#10003; Saving credentials...<br>Restarting device...
  </div>

  <div class="hint">
    After saving, the device restarts<br>and connects to your network.<br><br>
    Reconnect to your WiFi, then find<br>the device IP in your router or via<br>mDNS: <span style="color:var(--accent)">minihead.local</span>
  </div>
</div>

<script>
fetch('/wifi/scan').then(function(r){return r.json();}).then(function(nets){
  var el=document.getElementById('scanList');
  if(!nets||!nets.length){el.innerHTML='<div style="font-family:\'Courier New\',monospace;font-size:11px;color:#6b6b8a;padding:8px 0;">No networks found</div>';return;}
  el.innerHTML='';
  nets.forEach(function(n){
    var d=document.createElement('div');d.className='scan-item';
    var bars=n.rssi>-60?'▮▮▮':n.rssi>-75?'▮▮▯':'▮▯▯';
    d.innerHTML='<span>'+escH(n.ssid)+'</span><span class="scan-rssi">'+bars+(n.enc?' &#128274;':'')+'</span>';
    d.onclick=function(){document.getElementById('ssid').value=n.ssid;document.getElementById('pass').focus();};
    el.appendChild(d);
  });
}).catch(function(){
  document.getElementById('scanList').innerHTML='';
});
function escH(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function submitForm(e){
  e.preventDefault();
  var ssid=document.getElementById('ssid').value;
  var pass=document.getElementById('pass').value;
  document.getElementById('wifiForm').style.display='none';
  document.getElementById('scan-section').style.display='none';
  document.getElementById('savingMsg').style.display='block';
  fetch('/wifi/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)
  });
}
</script>
</body>
</html>
)=====";
