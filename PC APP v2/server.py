"""
MiniHead PC Leader v2
Uses waitress (or Flask dev fallback) for reliable concurrent handling.
"""
import json, os, socket, struct, threading, time, ipaddress, urllib.request
from flask import Flask, request, jsonify, send_from_directory, abort

# ── Constants ──────────────────────────────────────────────────────────────
BEACON_PORT   = 4210
CMD_PORT      = 4211
ARTNET_PORT   = 6454
HTTP_PORT     = 8080
PEER_TIMEOUT  = 90.0
KEEPALIVE_INT = 10.0
BEACON_INT    = 1.0
APP_VERSION   = "2.0"
OWN_MAC       = "00:00:00:00:00:PC"

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, "data")
os.makedirs(DATA_DIR, exist_ok=True)

DATA_FILE  = os.path.join(DATA_DIR, "v2_data.json")
CUES_FILE  = os.path.join(DATA_DIR, "v2_cues.json")
FLAGS_FILE = os.path.join(DATA_DIR, "v2_flags.json")

# ── Shared State ───────────────────────────────────────────────────────────
peers_lock    = threading.Lock()
peers         = {}          # mac → {mac, ip, fixID, name, mode, role, lastSeen}

fixtures_lock = threading.Lock()
fixtures      = {}          # id(int) → {id, name, mac|None}

cues_lock     = threading.Lock()
cues          = []          # [{id, name, r, g, b, w, pan, tilt, fixTargets}]
_next_cue_id  = 1

patches_lock  = threading.Lock()
patches       = []          # [{fixID, universe, startAddr}]

seq_lock      = threading.Lock()
seq           = {"running": False, "cue_ids": [], "interval_ms": 2000,
                 "loop": True, "index": 0, "last_step": 0.0}

artnet_lock   = threading.Lock()
artnet_dmx    = {}          # universe(int) → bytearray[512]
artnet_active = False
artnet_last   = {}          # "r","g","b","w","pan","tilt","patchCount"

patch_acks_lock = threading.Lock()
patch_acks      = {}        # fixID(int) → {"status":"pending"|"ok"|"timeout", "ts":float}

mac_names_lock = threading.Lock()
mac_names      = {}         # mac → friendly name

flags_lock = threading.Lock()
log_flags  = {                  # keys match ESP firmware log_config.h
    "artnetFrames":     False,  # per-frame R/G/B values (spammy)
    "artnetEvents":     True,   # ArtNet active / timeout / patch changed
    "discoveryBeacons": False,  # every beacon heard (spammy)
    "discoveryEvents":  True,   # role change, leader found/lost
    "udpVerbose":       True,   # UDP commands received / identify events
}

rainbow_lock   = threading.Lock()
rainbow_active = False

# ── Persistence ────────────────────────────────────────────────────────────
def _load_data():
    global fixtures, patches, mac_names, _next_cue_id
    if os.path.exists(DATA_FILE):
        try:
            with open(DATA_FILE) as f:
                d = json.load(f)
            with fixtures_lock:
                fixtures = {int(k): v for k, v in d.get("fixtures", {}).items()}
            with patches_lock:
                patches = d.get("patches", [])
            with mac_names_lock:
                mac_names = d.get("mac_names", {})
        except Exception as e:
            print(f"[data] load error: {e}")

    if os.path.exists(CUES_FILE):
        try:
            with open(CUES_FILE) as f:
                d = json.load(f)
            global cues
            with cues_lock:
                cues = d.get("cues", [])
                if cues:
                    _next_cue_id = max(c["id"] for c in cues) + 1
        except Exception as e:
            print(f"[cues] load error: {e}")

    if os.path.exists(FLAGS_FILE):
        try:
            with open(FLAGS_FILE) as f:
                d = json.load(f)
            with flags_lock:
                log_flags.update({k: bool(v) for k, v in d.items() if k in log_flags})
        except Exception as e:
            print(f"[flags] load error: {e}")

def _save_data():
    try:
        with fixtures_lock:
            fix_copy = dict(fixtures)
        with patches_lock:
            pat_copy = list(patches)
        with mac_names_lock:
            mn_copy = dict(mac_names)
        with open(DATA_FILE, "w") as f:
            json.dump({"fixtures": fix_copy, "patches": pat_copy,
                       "mac_names": mn_copy}, f, indent=2)
    except Exception as e:
        print(f"[data] save error: {e}")

def _save_cues():
    try:
        with cues_lock:
            c_copy = list(cues)
        with open(CUES_FILE, "w") as f:
            json.dump({"cues": c_copy}, f, indent=2)
    except Exception as e:
        print(f"[cues] save error: {e}")

def _save_flags():
    try:
        with flags_lock:
            fl_copy = dict(log_flags)
        with open(FLAGS_FILE, "w") as f:
            json.dump(fl_copy, f, indent=2)
    except Exception as e:
        print(f"[flags] save error: {e}")

# ── Helpers ────────────────────────────────────────────────────────────────
def _own_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

def _broadcast_addr(ip):
    try:
        iface = ipaddress.IPv4Interface(ip + "/24")
        return str(iface.network.broadcast_address)
    except Exception:
        return "255.255.255.255"

def _build_beacon():
    ip = _own_ip()
    return f"MINIHEAD|{OWN_MAC}|{ip}|0|LEADER|PC|PC".encode()

def _update_peer(mac, ip, fix_id, role, name, mode=""):
    with peers_lock:
        existed = mac in peers
        peers[mac] = {
            "mac":      mac,
            "ip":       ip,
            "fixID":    int(fix_id) if str(fix_id).isdigit() else 0,
            "name":     name,
            "role":     role,
            "mode":     mode,
            "lastSeen": time.time(),
        }
        if not existed:
            print(f"[beacon] Peer joined: {mac} {ip} fix={fix_id} role={role}")
            fid = int(fix_id) if str(fix_id).isdigit() else 0
            if fid > 0:
                with patches_lock:
                    match = next((p for p in patches if p["fixID"] == fid), None)
                if match:
                    threading.Thread(target=_push_patch_to_esp,
                                     args=(fid, match["universe"], match["startAddr"]),
                                     daemon=True).start()

def _expire_peers():
    now = time.time()
    with peers_lock:
        expired = [m for m, p in peers.items() if now - p["lastSeen"] > PEER_TIMEOUT]
        for m in expired:
            print(f"[beacon] Peer expired: {m}")
            del peers[m]

def _send_udp_cmd(ip, cmd, mac=None):
    try:
        msg = f"CMD|{mac or '*'}|{cmd}".encode()
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(msg, (ip, CMD_PORT))
        sock.close()
    except Exception as e:
        print(f"[udp] send error to {ip}: {e}")

def _send_udp_raw(ip, msg):
    """Send a raw UDP string to CMD_PORT (no CMD|mac| wrapper)."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(msg.encode(), (ip, CMD_PORT))
        sock.close()
    except Exception as e:
        print(f"[udp] raw send error to {ip}: {e}")

def _unicast_all(cmd):
    """Unicast cmd to every known peer — avoids Fritz!Box broadcast blocking (WiFi↔Ethernet)."""
    with peers_lock:
        targets = [(p["ip"], p["mac"]) for p in peers.values() if p.get("mac") != OWN_MAC]
    for ip, mac in targets:
        _send_udp_cmd(ip, cmd, mac)

def _broadcast_cmd(cmd):
    try:
        ip = _own_ip()
        bcast = _broadcast_addr(ip)
        msg = f"CMD|*|{cmd}".encode()
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.sendto(msg, (bcast, CMD_PORT))
        sock.close()
    except Exception as e:
        print(f"[udp] broadcast error: {e}")

def _push_patch_to_esp(fid, uni, addr):
    """Push a patch record to every online ESP whose fixID matches fid.
    The ESP stores it in /artnet.json so its native ArtNet receiver can apply it.
    Updates patch_acks so GET /api/artnet/patch/ack reflects the result."""
    with peers_lock:
        targets = [(p["ip"], p["mac"]) for p in peers.values()
                   if p.get("fixID") == fid and p.get("mac") != OWN_MAC]
    if not targets:
        return
    # Mark pending before attempting
    with patch_acks_lock:
        patch_acks[fid] = {"status": "pending", "ts": time.time()}
    success = False
    for ip, mac in targets:
        try:
            body = json.dumps({"fixID": fid, "universe": uni, "startAddr": addr}).encode()
            req  = urllib.request.Request(
                f"http://{ip}/api/artnet/patch",
                data=body, method="POST",
                headers={"Content-Type": "application/json"})
            urllib.request.urlopen(req, timeout=3)
            print(f"[patch] pushed Fix#{fid} U{uni} A{addr} → {mac} ({ip})")
            success = True
        except Exception as e:
            print(f"[patch] push failed → {ip}: {e}")
    with patch_acks_lock:
        patch_acks[fid] = {"status": "ok" if success else "timeout", "ts": time.time()}

def _delete_patch_from_esp(fid):
    """Tell the ESP to remove the patch for fid from its /artnet.json."""
    with peers_lock:
        targets = [(p["ip"], p["mac"]) for p in peers.values()
                   if p.get("fixID") == fid and p.get("mac") != OWN_MAC]
    for ip, mac in targets:
        try:
            req = urllib.request.Request(
                f"http://{ip}/api/artnet/patch/{fid}",
                method="DELETE")
            urllib.request.urlopen(req, timeout=3)
            print(f"[patch] deleted Fix#{fid} from {mac} ({ip})")
        except Exception as e:
            print(f"[patch] delete failed → {ip}: {e}")

def _send_command(cmd, targets=None):
    """Send command to a list of MACs, or unicast to all peers if targets is empty/None."""
    if targets:
        with peers_lock:
            peers_copy = dict(peers)
        for mac in targets:
            if mac == "00:00:00:00:00:PC":
                continue
            if mac in peers_copy:
                _send_udp_cmd(peers_copy[mac]["ip"], cmd, mac)
    else:
        _unicast_all(cmd)

def _fire_cue(cue):
    cmd = f"R:{cue['r']},G:{cue['g']},B:{cue['b']},W:{cue['w']},PAN:{cue['pan']},TILT:{cue['tilt']}"
    ft = cue.get("fixTargets", [0])
    if not ft or ft == [0]:
        _unicast_all(cmd)
    else:
        with fixtures_lock:
            fix_copy = dict(fixtures)
        with peers_lock:
            peers_copy = dict(peers)
        macs = []
        for fid in ft:
            fix = fix_copy.get(fid)
            if fix and fix.get("mac"):
                macs.append(fix["mac"])
        if macs:
            for mac in macs:
                if mac in peers_copy:
                    _send_udp_cmd(peers_copy[mac]["ip"], cmd, mac)
        else:
            _unicast_all(cmd)

# ── Background Threads ─────────────────────────────────────────────────────
def beacon_sender():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    while True:
        try:
            msg = _build_beacon()
            ip  = _own_ip()
            bcast = _broadcast_addr(ip)
            sock.sendto(msg, (bcast, BEACON_PORT))
            with peers_lock:
                peer_ips = [p["ip"] for m, p in peers.items() if m != OWN_MAC]
            for pip in peer_ips:
                try:
                    sock.sendto(msg, (pip, BEACON_PORT))
                except Exception:
                    pass
            # Keep the PC's own entry alive — beacon_receiver ignores OWN_MAC so refresh manually
            with peers_lock:
                if OWN_MAC in peers:
                    peers[OWN_MAC]["lastSeen"] = time.time()
        except Exception as e:
            print(f"[beacon_sender] error: {e}")
        time.sleep(BEACON_INT)

def beacon_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError:
        pass
    sock.bind(("", BEACON_PORT))
    sock.settimeout(2.0)
    while True:
        try:
            data, addr = sock.recvfrom(256)
            text = data.decode(errors="ignore").strip()
            parts = text.split("|")
            if len(parts) >= 5 and parts[0] == "MINIHEAD":
                mac    = parts[1]
                ip     = parts[2]
                fix_id = parts[3]
                role   = parts[4]
                name   = parts[5] if len(parts) > 5 else mac[-5:]
                mode   = parts[6] if len(parts) > 6 else ""
                if mac != OWN_MAC:
                    _update_peer(mac, ip, fix_id, role, name, mode)
            _expire_peers()
        except socket.timeout:
            _expire_peers()
        except Exception as e:
            print(f"[beacon_receiver] error: {e}")

def http_keepalive():
    """Poll /api/status on each peer every KEEPALIVE_INT seconds to keep them alive
    through Fritz!Box WiFi→Ethernet UDP blocking."""
    import urllib.request
    while True:
        time.sleep(KEEPALIVE_INT)
        with peers_lock:
            peer_list = [(m, p["ip"]) for m, p in peers.items() if m != OWN_MAC]
        for mac, ip in peer_list:
            try:
                url = f"http://{ip}/api/status"
                req = urllib.request.urlopen(url, timeout=3)
                req.read()
                req.close()
                with peers_lock:
                    if mac in peers:
                        peers[mac]["lastSeen"] = time.time()
                print(f"[keepalive] ok {mac} {ip}")
            except Exception as e:
                print(f"[keepalive] {mac} {ip} — {e}")

def sequencer_runner():
    while True:
        time.sleep(0.1)
        with seq_lock:
            if not seq["running"]:
                continue
            now = time.time()
            if (now - seq["last_step"]) * 1000 < seq["interval_ms"]:
                continue
            seq["last_step"] = now
            ids = seq["cue_ids"]
            if not ids:
                seq["running"] = False
                continue
            idx = seq["index"] % len(ids)
            cue_id = ids[idx]
            seq["index"] = idx + 1
            if seq["index"] >= len(ids):
                if seq["loop"]:
                    seq["index"] = 0
                else:
                    seq["running"] = False

        with cues_lock:
            cue = next((c for c in cues if c["id"] == cue_id), None)
        if cue:
            _fire_cue(cue)

def artnet_sniffer():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError:
        pass
    sock.bind(("", ARTNET_PORT))
    sock.settimeout(1.0)
    global artnet_active
    last_packet = 0.0
    while True:
        try:
            data, addr = sock.recvfrom(600)
            # Minimal Art-Net DMX parse: header "Art-Net\0", OpCode 0x5000
            if len(data) < 18 or data[:7] != b"Art-Net":
                continue
            opcode = struct.unpack_from("<H", data, 8)[0]
            if opcode != 0x5000:
                continue
            universe = struct.unpack_from("<H", data, 14)[0]
            length   = struct.unpack_from(">H", data, 16)[0]
            dmx_data = data[18:18+length]
            with artnet_lock:
                artnet_dmx[universe] = bytearray(dmx_data)
            last_packet = time.time()
            # Update artnet_last from patches
            _artnet_apply_dmx()
        except socket.timeout:
            pass
        except Exception as e:
            print(f"[artnet] error: {e}")

        with artnet_lock:
            was = artnet_active
            artnet_active = (time.time() - last_packet) < 3.0
            if was and not artnet_active:
                print("[artnet] stream stopped")
            elif not was and artnet_active:
                print("[artnet] stream started")

def _artnet_apply_dmx():
    """Read DMX values for each patched fixture and send commands. Called from artnet_sniffer."""
    with patches_lock:
        pat_copy = list(patches)
    with fixtures_lock:
        fix_copy = dict(fixtures)
    with peers_lock:
        peers_copy = dict(peers)
    with artnet_lock:
        dmx_copy = {u: bytes(b) for u, b in artnet_dmx.items()}

    patch_count = len(pat_copy)
    # For global artnet_last display, aggregate first fixture
    first = True
    for patch in pat_copy:
        fid  = patch["fixID"]
        uni  = patch["universe"]
        addr = patch["startAddr"] - 1  # 0-indexed
        dmx  = dmx_copy.get(uni, b"")
        if len(dmx) < addr + 7:
            continue
        master = dmx[addr]          # CH_MASTER (0) — scales R/G/B/W
        r   = dmx[addr+1] * master // 255   # CH_RED   (1)
        g   = dmx[addr+2] * master // 255   # CH_GREEN (2)
        b   = dmx[addr+3] * master // 255   # CH_BLUE  (3)
        w   = dmx[addr+4] * master // 255   # CH_WHITE (4)
        pan = round(dmx[addr+5] / 255 * 270)  # CH_PAN  (5)
        tilt= round(dmx[addr+6] / 255 * 270)  # CH_TILT (6)
        cmd = f"R:{r},G:{g},B:{b},W:{w},PAN:{pan},TILT:{tilt}"
        if first:
            with artnet_lock:
                artnet_last.update(r=r, g=g, b=b, w=w, pan=pan, tilt=tilt,
                                   master=master, patchCount=patch_count)
            first = False
        # Routing: try fixture MAC → peer fixID match → unicast all peers (avoids Fritz!Box broadcast block)
        target_ip  = None
        target_mac = None
        fix = fix_copy.get(fid)
        if fix and fix.get("mac") and fix["mac"] in peers_copy:
            target_ip  = peers_copy[fix["mac"]]["ip"]
            target_mac = fix["mac"]
        if target_ip is None:
            for mac, peer in peers_copy.items():
                if mac != OWN_MAC and peer.get("fixID") == fid:
                    target_ip  = peer["ip"]
                    target_mac = mac
                    break
        if target_ip:
            _send_udp_cmd(target_ip, cmd, target_mac)
        else:
            # No specific target known — unicast to every peer (safe even through Fritz!Box)
            for mac, peer in peers_copy.items():
                if mac != OWN_MAC:
                    _send_udp_cmd(peer["ip"], cmd, mac)

# ── Flask App ──────────────────────────────────────────────────────────────
app = Flask(__name__, static_folder=None)

# ── Static files ────────────────────────────────────────────────────────────
@app.route("/")
def index():
    return send_from_directory(BASE_DIR, "index.html")

@app.route("/css/<path:fname>")
def css_file(fname):
    return send_from_directory(os.path.join(BASE_DIR, "css"), fname)

@app.route("/js/<path:fname>")
def js_file(fname):
    return send_from_directory(os.path.join(BASE_DIR, "js"), fname)

# ── Info ────────────────────────────────────────────────────────────────────
@app.route("/api/status")
def api_status():
    return jsonify({"status": "ok", "ip": _own_ip(), "role": "LEADER"})

@app.route("/api/version")
def api_version():
    return jsonify({"version": APP_VERSION})

# ── Heads ────────────────────────────────────────────────────────────────────
@app.route("/api/heads")
def api_heads():
    _expire_peers()
    with peers_lock:
        # Deep-copy so we never mutate the live peer dicts
        peer_list = [dict(p) for p in peers.values() if p["mac"] != OWN_MAC]

    with patches_lock:
        pat_copy = list(patches)
    with fixtures_lock:
        fix_copy = dict(fixtures)

    # Always include PC itself at index 0
    result = [{
        "mac":      OWN_MAC,
        "ip":       _own_ip(),
        "fixID":    0,
        "name":     "PC",
        "role":     "LEADER",
        "mode":     "",
        "lastSeen": time.time(),
        "online":   True,
        "universe":  None,
        "startAddr": None,
    }]

    for h in peer_list:
        h["online"] = True
        fid   = _fix_id_for_mac(h["mac"], fix_copy)
        patch = next((p for p in pat_copy if fid is not None and fid == p["fixID"]), None)
        h["universe"]  = patch["universe"]  if patch else None
        h["startAddr"] = patch["startAddr"] if patch else None
        result.append(h)

    return jsonify(result)

def _fix_id_for_mac(mac, fix_copy):
    for fid, f in fix_copy.items():
        if f.get("mac") == mac:
            return fid
    return None

@app.route("/api/heads/<mac>/fixid", methods=["POST"])
def api_head_set_fixid(mac):
    data = request.get_json(force=True)
    new_id = int(data.get("fixID", 0))
    with peers_lock:
        if mac in peers:
            peers[mac]["fixID"] = new_id
    # push to ESP
    with peers_lock:
        ip = peers.get(mac, {}).get("ip")
    if ip:
        try:
            import urllib.request
            url = f"http://{ip}/api/config/fixid"
            req = urllib.request.Request(url,
                data=json.dumps({"fixID": new_id}).encode(),
                headers={"Content-Type": "application/json"},
                method="POST")
            urllib.request.urlopen(req, timeout=3).read()
        except Exception as e:
            print(f"[fixid push] {mac}: {e}")
    return jsonify({"status": "ok"})

@app.route("/api/heads/<mac>/name", methods=["POST"])
def api_head_set_name(mac):
    data = request.get_json(force=True)
    name = str(data.get("name", ""))[:32]
    with mac_names_lock:
        mac_names[mac] = name
    with peers_lock:
        if mac in peers:
            peers[mac]["name"] = name
    _save_data()
    return jsonify({"status": "ok"})

@app.route("/api/heads/<mac>/identify", methods=["POST"])
def api_head_identify(mac):
    data = request.get_json(force=True, silent=True) or {}
    on   = data.get("on", True)
    with peers_lock:
        ip = peers.get(mac, {}).get("ip")
    if ip:
        _send_udp_raw(ip, f"IDENTIFY_ON|{mac}" if on else f"IDENTIFY_OFF|{mac}")
    return jsonify({"status": "ok"})

# ── Fixtures ──────────────────────────────────────────────────────────────────
@app.route("/api/fixtures")
def api_fixtures():
    with fixtures_lock:
        result = sorted(fixtures.values(), key=lambda x: x["id"])
    with peers_lock:
        online_macs = {p["mac"] for p in peers.values()}
    for f in result:
        f["online"] = f.get("mac") in online_macs
    return jsonify(result)

@app.route("/api/fixtures", methods=["POST"])
def api_fixture_create():
    global _next_cue_id
    data = request.get_json(force=True)
    fid  = data.get("id")
    name = str(data.get("name", "")).strip()[:32] or f"Fix {fid}"
    mac  = data.get("mac") or None
    if not fid:
        return jsonify({"error": "id required"}), 400
    fid = int(fid)
    with fixtures_lock:
        fixtures[fid] = {"id": fid, "name": name, "mac": mac}
    _save_data()
    return jsonify({"status": "ok", "id": fid})

@app.route("/api/fixtures/<int:fid>", methods=["PUT"])
def api_fixture_update(fid):
    data = request.get_json(force=True)
    with fixtures_lock:
        if fid not in fixtures:
            return jsonify({"error": "not found"}), 404
        if "name" in data:
            fixtures[fid]["name"] = str(data["name"])[:32]
        if "mac" in data:
            fixtures[fid]["mac"] = data["mac"] or None
    _save_data()
    return jsonify({"status": "ok"})

@app.route("/api/fixtures/<int:fid>", methods=["DELETE"])
def api_fixture_delete(fid):
    with fixtures_lock:
        fixtures.pop(fid, None)
    _save_data()
    return jsonify({"status": "ok"})

# ── Commands ──────────────────────────────────────────────────────────────────
@app.route("/api/send", methods=["POST"])
def api_send():
    data    = request.get_json(force=True)
    cmd     = str(data.get("command", "")).strip()
    targets = data.get("targets", [])
    if not cmd:
        return jsonify({"error": "no command"}), 400
    _send_command(cmd, targets if targets else None)
    return jsonify({"status": "ok"})

@app.route("/api/rainbow", methods=["POST"])
def api_rainbow():
    global rainbow_active
    data = request.get_json(force=True)
    on   = bool(data.get("on", False))
    with rainbow_lock:
        rainbow_active = on
    cmd = "RAINBOW:1" if on else "RAINBOW:0"
    _unicast_all(cmd)
    return jsonify({"status": "ok", "on": on})

# ── Cues ──────────────────────────────────────────────────────────────────────
@app.route("/api/cues")
def api_cues_list():
    with cues_lock:
        return jsonify(list(cues))

@app.route("/api/cues", methods=["POST"])
def api_cue_create():
    global _next_cue_id
    data = request.get_json(force=True)
    name = str(data.get("name", "")).strip()[:64]
    if not name:
        return jsonify({"error": "name required"}), 400
    cue = {
        "id":         _next_cue_id,
        "name":       name,
        "r":          int(data.get("r", 0)),
        "g":          int(data.get("g", 0)),
        "b":          int(data.get("b", 0)),
        "w":          int(data.get("w", 0)),
        "pan":        int(data.get("pan", 90)),
        "tilt":       int(data.get("tilt", 45)),
        "fixTargets": data.get("fixTargets", [0]),
    }
    _next_cue_id += 1
    with cues_lock:
        cues.append(cue)
    _save_cues()
    return jsonify({"status": "ok", "id": cue["id"]})

@app.route("/api/cues/<int:cid>", methods=["DELETE"])
def api_cue_delete(cid):
    with cues_lock:
        idx = next((i for i, c in enumerate(cues) if c["id"] == cid), None)
        if idx is not None:
            cues.pop(idx)
    _save_cues()
    return jsonify({"status": "ok"})

@app.route("/api/cues/<int:cid>/fire", methods=["POST"])
def api_cue_fire(cid):
    with cues_lock:
        cue = next((c for c in cues if c["id"] == cid), None)
    if not cue:
        return jsonify({"error": "not found"}), 404
    _fire_cue(cue)
    return jsonify({"status": "ok"})

@app.route("/api/cues/<int:cid>/targets", methods=["PUT"])
def api_cue_targets(cid):
    data = request.get_json(force=True)
    ft   = data.get("fixTargets", [0])
    with cues_lock:
        cue = next((c for c in cues if c["id"] == cid), None)
        if not cue:
            return jsonify({"error": "not found"}), 404
        cue["fixTargets"] = ft
    _save_cues()
    return jsonify({"status": "ok"})

@app.route("/api/cues/reorder", methods=["PUT"])
def api_cues_reorder():
    data  = request.get_json(force=True)
    order = data.get("order", [])
    with cues_lock:
        idx = {c["id"]: c for c in cues}
        new = [idx[oid] for oid in order if oid in idx]
        cues[:] = new
    _save_cues()
    return jsonify({"status": "ok"})

# ── Sequencer ──────────────────────────────────────────────────────────────────
@app.route("/api/sequencer/start", methods=["POST"])
def api_seq_start():
    data = request.get_json(force=True)
    with seq_lock:
        seq["cue_ids"]     = list(data.get("cue_ids", []))
        seq["interval_ms"] = int(data.get("interval_ms", 2000))
        seq["loop"]        = bool(data.get("loop", True))
        seq["index"]       = 0
        seq["last_step"]   = 0.0
        seq["running"]     = True
    return jsonify({"status": "ok"})

@app.route("/api/sequencer/stop", methods=["POST"])
def api_seq_stop():
    with seq_lock:
        seq["running"] = False
    return jsonify({"status": "ok"})

@app.route("/api/sequencer/status")
def api_seq_status():
    with seq_lock:
        return jsonify(dict(seq))

# ── Art-Net ──────────────────────────────────────────────────────────────────
@app.route("/api/artnet/status")
def api_artnet_status():
    with artnet_lock:
        active = artnet_active
        last   = dict(artnet_last)
    with patches_lock:
        patch_count = len(patches)
    return jsonify({
        "active":     active,
        "r":          last.get("r", 0),
        "g":          last.get("g", 0),
        "b":          last.get("b", 0),
        "w":          last.get("w", 0),
        "pan":        last.get("pan", 0),
        "tilt":       last.get("tilt", 0),
        "patchCount": patch_count,
    })

@app.route("/api/artnet/patch")
def api_artnet_patch_list():
    with patches_lock:
        return jsonify(list(patches))

@app.route("/api/artnet/patch/ack")
def api_artnet_patch_ack():
    """Per-fixID push acknowledgement state.
    Used by the ESP's discovery panel JS to show ✓/⟳/✗ icons next to patches.
    Returns: {"<fixID>": {"status": "ok"|"pending"|"timeout", "ts": <float>}, ...}
    """
    with patch_acks_lock:
        return jsonify({str(k): v for k, v in patch_acks.items()})

@app.route("/api/artnet/patch", methods=["POST"])
def api_artnet_patch_create():
    data  = request.get_json(force=True)
    fid   = int(data.get("fixID", 0))
    uni   = int(data.get("universe", 0))
    addr  = int(data.get("startAddr", 1))
    with patches_lock:
        patches[:] = [p for p in patches if p["fixID"] != fid]
        patches.append({"fixID": fid, "universe": uni, "startAddr": addr})
    _save_data()
    threading.Thread(target=_push_patch_to_esp, args=(fid, uni, addr), daemon=True).start()
    return jsonify({"status": "ok"})

@app.route("/api/artnet/patch/<int:fid>", methods=["PUT"])
def api_artnet_patch_update(fid):
    data = request.get_json(force=True)
    with patches_lock:
        patch = next((p for p in patches if p["fixID"] == fid), None)
        if not patch:
            return jsonify({"error": "not found"}), 404
        if "universe"  in data: patch["universe"]  = int(data["universe"])
        if "startAddr" in data: patch["startAddr"] = int(data["startAddr"])
        uni, addr = patch["universe"], patch["startAddr"]
    _save_data()
    threading.Thread(target=_push_patch_to_esp, args=(fid, uni, addr), daemon=True).start()
    return jsonify({"status": "ok"})

@app.route("/api/artnet/patch/<int:fid>", methods=["DELETE"])
def api_artnet_patch_delete(fid):
    with patches_lock:
        patches[:] = [p for p in patches if p["fixID"] != fid]
    _save_data()
    threading.Thread(target=_delete_patch_from_esp, args=(fid,), daemon=True).start()
    return jsonify({"status": "ok"})

@app.route("/api/artnet/patch", methods=["DELETE"])
def api_artnet_patch_clear():
    with patches_lock:
        patches.clear()
    _save_data()
    return jsonify({"status": "ok"})

@app.route("/api/artnet/patch/bulk", methods=["POST"])
def api_artnet_patch_bulk():
    data  = request.get_json(force=True)
    items = data.get("patches", [])
    new_patches = []
    for item in items:
        fid  = int(item.get("fixID",     0))
        uni  = int(item.get("universe",  0))
        addr = int(item.get("startAddr", 1))
        if fid > 0:
            new_patches.append({"fixID": fid, "universe": uni, "startAddr": addr})
    with patches_lock:
        patches.clear()
        patches.extend(new_patches)
    _save_data()
    # Push each patch to its ESP individually (same protocol as single-patch create)
    for p in new_patches:
        threading.Thread(target=_push_patch_to_esp,
                         args=(p["fixID"], p["universe"], p["startAddr"]),
                         daemon=True).start()
    return jsonify({"status": "ok", "count": len(new_patches)})

# ── Log Config ────────────────────────────────────────────────────────────────
@app.route("/api/logconfig", methods=["GET", "POST"])
def api_logconfig():
    if request.method == "GET":
        with flags_lock:
            return jsonify(dict(log_flags))
    data = request.get_json(force=True)
    with flags_lock:
        for k in log_flags:
            if k in data:
                log_flags[k] = bool(data[k])
    _save_flags()
    return jsonify({"status": "ok"})

@app.route("/api/esp/logconfig", methods=["GET", "POST"])
def api_esp_logconfig():
    """Push log config to all connected ESPs."""
    if request.method == "GET":
        with flags_lock:
            return jsonify(dict(log_flags))
    data = request.get_json(force=True)
    with flags_lock:
        for k in log_flags:
            if k in data:
                log_flags[k] = bool(data[k])
    _save_flags()
    # Push to all ESPs
    with peers_lock:
        peer_list = [(m, p["ip"]) for m, p in peers.items() if m != OWN_MAC]
    import urllib.request as ureq
    with flags_lock:
        payload = json.dumps(dict(log_flags)).encode()
    for mac, ip in peer_list:
        try:
            url = f"http://{ip}/api/logconfig"
            req = ureq.Request(url, data=payload,
                               headers={"Content-Type": "application/json"},
                               method="POST")
            ureq.urlopen(req, timeout=3).read()
        except Exception as e:
            print(f"[logconfig push] {mac} {ip}: {e}")
    return jsonify({"status": "ok"})

# ── Startup ────────────────────────────────────────────────────────────────────
def _start_threads():
    for target in [beacon_sender, beacon_receiver, http_keepalive,
                   sequencer_runner, artnet_sniffer]:
        t = threading.Thread(target=target, daemon=True, name=target.__name__)
        t.start()
        print(f"[init] thread started: {target.__name__}")

if __name__ == "__main__":
    print(f"MiniHead PC Leader v{APP_VERSION} starting on :{HTTP_PORT}")
    print(f"Own IP: {_own_ip()}")
    _load_data()
    # Register self as peer
    _update_peer(OWN_MAC, _own_ip(), 0, "LEADER", "PC")
    _start_threads()
    try:
        from waitress import serve
        print("[server] using waitress")
        serve(app, host="0.0.0.0", port=HTTP_PORT, threads=8)
    except ImportError:
        print("[server] waitress not found, using Flask dev server (threaded)")
        app.run(host="0.0.0.0", port=HTTP_PORT, debug=False, threaded=True)
