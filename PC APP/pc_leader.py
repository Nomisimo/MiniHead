#!/usr/bin/env python3
"""
pc_leader.py — MiniHead PC Leader  [PC App v4.2]
Runs on your Mac/PC. Acts as the network leader:
  - Sends UDP beacons every second
  - Collects peer beacons → builds peer table
  - Serves the Web UI + all API endpoints
  - Sends UDP commands to follower ESP32 nodes

Usage:
  pip install flask
  python3 pc_leader.py

Then open http://localhost:8080 in your browser.
"""

import socket
import struct
import threading
import time
import json
import os
import urllib.request
from flask import Flask, request, jsonify, send_from_directory

# ── Version ───────────────────────────────────────────────────────
APP_VERSION = "PC App v4.2"

# ── Config ────────────────────────────────────────────────────────
BEACON_PORT   = 4210
CMD_PORT      = 4211
BEACON_INTERVAL = 1.0       # seconds
PEER_TIMEOUT    = 12.0      # seconds before peer is removed (ESP32 beacons every 2s)
OWN_FIX_ID      = 0         # set your fixture ID here
CUES_FILE         = "pc_cues.json"
FIXTURES_FILE     = "pc_fixtures.json"
ARTNET_PATCH_FILE = "pc_artnet_patch.json"

# ── Own identity ──────────────────────────────────────────────────
def get_own_mac():
    # Use a fixed "MAC" so we always win leader election (00:00:... is smallest)
    return "00:00:00:00:00:PC"

def get_own_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

def get_broadcast_addr(ip: str) -> str:
    """Derive subnet broadcast from IP — assumes /24."""
    return ip.rsplit(".", 1)[0] + ".255"

OWN_MAC        = get_own_mac()
OWN_IP         = get_own_ip()
OWN_ROLE       = "LEADER"
BROADCAST_ADDR = get_broadcast_addr(OWN_IP)

# ── Peer table ────────────────────────────────────────────────────
peers_lock = threading.Lock()
peers = {}  # mac -> {mac, ip, fixID, name, role, lastSeen}

def update_peer(mac, ip, fix_id, role, name=""):
    should_push = False
    with peers_lock:
        is_new      = mac not in peers
        # Re-push patches when a known peer was silent for >6 s (likely rebooted)
        # or came back on a different IP (DHCP change / genuine reconnect).
        was_silent  = (not is_new and
                       time.time() - peers[mac]["lastSeen"] > PEER_TIMEOUT * 0.5)
        ip_changed  = (not is_new and peers[mac]["ip"] != ip)
        should_push = is_new or was_silent or ip_changed

        if is_new:
            print(f"[Discovery] Peer joined:       {mac}  IP:{ip}  Fix#{fix_id}  \"{name}\"  {role}")
        elif ip_changed:
            print(f"[Discovery] Peer reconnected:  {mac}  new IP:{ip}  Fix#{fix_id}")
        elif was_silent:
            print(f"[Discovery] Peer re-appeared:  {mac}  Fix#{fix_id}")

        peers[mac] = {
            "mac":      mac,
            "ip":       ip,
            "fixID":    int(fix_id) if str(fix_id).isdigit() else 0,
            "name":     name,
            "role":     "LEADER" if role in ("LEADER", "1") else "FOLLOWER",
            "lastSeen": time.time()
        }
    # Auto-register in fixture pool if fixID assigned
    fid = int(fix_id) if str(fix_id).isdigit() else 0
    if fid > 0:
        _fixture_auto_register(fid, name, mac)
        if should_push:
            _push_patch_on_join(fid, ip, mac)

def expire_peers():
    now = time.time()
    with peers_lock:
        expired = [m for m, p in peers.items() if now - p["lastSeen"] > PEER_TIMEOUT]
        for m in expired:
            print(f"[Discovery] Peer expired: {m}")
            del peers[m]

def get_peers():
    with peers_lock:
        return list(peers.values())

# ── Fixture pool ──────────────────────────────────────────────────
fixtures_lock = threading.Lock()
fixtures = {}  # id(int) -> {id, name, mac}

def load_fixtures():
    global fixtures
    if os.path.exists(FIXTURES_FILE):
        with open(FIXTURES_FILE) as f:
            data = json.load(f)
        fixtures = {int(fx["id"]): fx for fx in data}
        print(f"[Fixtures] Loaded {len(fixtures)} fixtures")

def save_fixtures():
    with open(FIXTURES_FILE, "w") as f:
        json.dump(list(fixtures.values()), f, indent=2)

def _fixture_auto_register(fid, name, mac):
    """Called when a peer beacons in with a fixID — keeps pool in sync."""
    with fixtures_lock:
        if fid not in fixtures:
            fixtures[fid] = {"id": fid, "name": name, "mac": mac}
            save_fixtures()
        else:
            changed = False
            if not fixtures[fid].get("mac") and mac:
                fixtures[fid]["mac"] = mac; changed = True
            if name and not fixtures[fid].get("name"):
                fixtures[fid]["name"] = name; changed = True
            if changed:
                save_fixtures()

def get_fixtures_with_status():
    peer_list = get_peers()
    by_mac   = {p["mac"]: p for p in peer_list}
    by_fixid = {p["fixID"]: p for p in peer_list if p.get("fixID", 0) > 0}
    result = []
    with fixtures_lock:
        for fid, fx in sorted(fixtures.items()):
            p = by_mac.get(fx.get("mac", "")) or by_fixid.get(fid)
            online = p is not None
            result.append({**fx, "online": online, "ip": p["ip"] if online else None})
    return result

# ── Cue storage ───────────────────────────────────────────────────
cues_lock = threading.Lock()
cues = []

def load_cues():
    global cues
    if os.path.exists(CUES_FILE):
        with open(CUES_FILE) as f:
            cues = json.load(f)
        print(f"[Cues] Loaded {len(cues)} cues")

def save_cues():
    with open(CUES_FILE, "w") as f:
        json.dump(cues, f, indent=2)

# ── Art-Net patch storage ─────────────────────────────────────────
# Each patch: {"fixID": int, "universe": int, "startAddr": int}
artnet_patches_lock = threading.Lock()
artnet_patches = []  # list of patch dicts

def load_artnet_patches():
    global artnet_patches
    if os.path.exists(ARTNET_PATCH_FILE):
        with open(ARTNET_PATCH_FILE) as f:
            artnet_patches = json.load(f)
        print(f"[ArtNet] Loaded {len(artnet_patches)} patch(es)")

def save_artnet_patches():
    with open(ARTNET_PATCH_FILE, "w") as f:
        json.dump(artnet_patches, f, indent=2)

# ── Art-Net sniffer ───────────────────────────────────────────────
# Binds to UDP 6454 with SO_REUSEADDR so it co-exists with any other
# Art-Net software. Catches broadcast ArtDmx packets; unicast-only setups
# fall back to "no activity" (the ESP's own /api/artnet/status still works
# correctly from the ESP's web UI).
_artnet_lock       = threading.Lock()
_artnet_last_pkt   = 0.0          # epoch of last valid ArtDmx packet
_artnet_dmx        = {}           # universe(int) → list[512]
ARTNET_TIMEOUT     = 8.0          # seconds without packet → inactive

def _parse_artdmx(data: bytes):
    """Return (universe, dmx_list) from an ArtDmx UDP payload, or None."""
    if len(data) < 18:                                    return None
    if data[:8] != b"Art-Net\x00":                       return None
    if struct.unpack_from("<H", data, 8)[0] != 0x5000:   return None
    universe = struct.unpack_from("<H", data, 14)[0]
    length   = struct.unpack_from(">H", data, 16)[0]
    if len(data) < 18 + length:                          return None
    return universe, list(data[18: 18 + length])

def artnet_sniffer():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try: sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError: pass   # Windows / older Linux
    sock.bind(("", 6454))
    sock.settimeout(1.0)
    print(f"[ArtNet] Sniffer on UDP 6454 (broadcast only)")
    while True:
        try:
            data, _ = sock.recvfrom(600)
            result = _parse_artdmx(data)
            if result:
                uni, dmx = result
                with _artnet_lock:
                    _artnet_last_pkt     = time.time()
                    _artnet_dmx[uni]     = dmx
        except socket.timeout:
            pass
        except Exception as e:
            print(f"[ArtNet] Sniffer error: {e}")

def get_artnet_status() -> dict:
    """Active flag + live channel values decoded from the first stored patch."""
    with _artnet_lock:
        active   = (_artnet_last_pkt > 0 and
                    time.time() - _artnet_last_pkt < ARTNET_TIMEOUT)
        dmx_snap = dict(_artnet_dmx)

    r = g = b = w = pan = tilt = 0
    if active:
        with artnet_patches_lock:
            patch = artnet_patches[0] if artnet_patches else None
        if patch:
            uni  = patch["universe"]
            base = patch["startAddr"] - 1       # 0-based
            dmx  = dmx_snap.get(uni, [])
            if len(dmx) >= base + 7:
                master = dmx[base + 0]
                r    = dmx[base + 1] * master // 255
                g    = dmx[base + 2] * master // 255
                b    = dmx[base + 3] * master // 255
                w    = dmx[base + 4] * master // 255
                pan  = dmx[base + 5] * 180 // 255
                tilt = dmx[base + 6] * 180 // 255

    with artnet_patches_lock:
        pc = len(artnet_patches)

    return {"active": active, "patchCount": pc,
            "r": r, "g": g, "b": b, "w": w, "pan": pan, "tilt": tilt}

# ── Art-Net patch push helpers ────────────────────────────────────

def push_patch_to_esp(fix_id: int, universe: int, start_addr: int):
    """Send SETPATCH UDP to the ESP that owns this fixID (if online)."""
    for p in get_peers():
        if int(p.get("fixID", 0)) == fix_id:
            udp_send(p["ip"], p["mac"], f"SETPATCH:{fix_id},{universe},{start_addr}")
            print(f"[ArtNet] Pushed SETPATCH:{fix_id},{universe},{start_addr} → {p['ip']}")
            return
    print(f"[ArtNet] Fix#{fix_id} not online — patch stored locally only")

def _push_patch_on_join(fid: int, ip: str, mac: str):
    """When a peer joins, push its stored patch so it has correct Art-Net mapping."""
    with artnet_patches_lock:
        patch = next((p for p in artnet_patches if p["fixID"] == fid), None)
    if patch:
        udp_send(ip, mac, f"SETPATCH:{fid},{patch['universe']},{patch['startAddr']}")
        print(f"[ArtNet] Join-sync SETPATCH:{fid},{patch['universe']},{patch['startAddr']} → {ip}")

# ── UDP beacon sender ─────────────────────────────────────────────
def beacon_sender():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    while True:
        pkt = f"MINIHEAD|{OWN_MAC}|{OWN_IP}|{OWN_FIX_ID}|LEADER|PC"
        try:
            sock.sendto(pkt.encode(), (BROADCAST_ADDR, BEACON_PORT))
        except Exception as e:
            print(f"[Beacon] Send error: {e}")
        expire_peers()
        time.sleep(BEACON_INTERVAL)

# ── UDP beacon receiver ───────────────────────────────────────────
def beacon_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", BEACON_PORT))
    sock.settimeout(1.0)
    while True:
        try:
            data, addr = sock.recvfrom(256)
            msg = data.decode().strip()
            if not msg.startswith("MINIHEAD|"):
                continue
            parts = msg.split("|")
            if len(parts) < 5:
                continue
            _, mac, ip, fix_id, role = parts[:5]
            name = parts[5] if len(parts) > 5 else ""
            if mac == OWN_MAC:
                continue  # ignore own beacon
            update_peer(mac, ip, fix_id, role, name)
        except socket.timeout:
            pass
        except Exception as e:
            print(f"[Beacon RX] Error: {e}")

# ── UDP command sender ────────────────────────────────────────────
_cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def udp_send(ip, target_mac, command):
    pkt = f"CMD|{target_mac}|{command}"
    try:
        _cmd_sock.sendto(pkt.encode(), (ip, CMD_PORT))
    except Exception as e:
        print(f"[UDP] Send error to {ip}: {e}")

def udp_identify(ip, target_mac, on: bool):
    kind = "IDENTIFY_ON" if on else "IDENTIFY_OFF"
    pkt  = f"{kind}|{target_mac}"
    try:
        _cmd_sock.sendto(pkt.encode(), (ip, CMD_PORT))
    except Exception as e:
        print(f"[UDP] Identify error to {ip}: {e}")

def udp_broadcast(command):
    for p in get_peers():
        udp_send(p["ip"], p["mac"], command)

# ── Flask app ─────────────────────────────────────────────────────
app = Flask(__name__)

# ── Serve Web UI ──────────────────────────────────────────────────
@app.route("/")
def serve_ui():
    return send_from_directory(".", "index.html")

@app.route("/plugins/wifi/discovery_panel.html")
def serve_discovery_panel():
    return send_from_directory("plugins/wifi", "discovery_panel.html")

@app.route("/plugins/artnet/artnet_panel.html")
def serve_artnet_panel():
    return send_from_directory("plugins/artnet", "artnet_panel.html")

# ── /api/version ─────────────────────────────────────────────────
@app.route("/api/version")
def api_version():
    return jsonify({"version": APP_VERSION})

# ── /api/status ───────────────────────────────────────────────────
@app.route("/api/status")
def api_status():
    return jsonify({"connected": True, "port": "PC", "ip": OWN_IP})

# ── /api/heads ────────────────────────────────────────────────────
@app.route("/api/heads")
def api_get_heads():
    result = [{
        "mac":   OWN_MAC,
        "ip":    OWN_IP,
        "fixID": OWN_FIX_ID,
        "name":  "PC",
        "role":  "LEADER",
        "self":  True
    }]
    for p in get_peers():
        result.append({**p, "self": False})
    return jsonify(result)

@app.route("/api/heads/<mac>/fixid", methods=["POST"])
def api_set_fixid(mac):
    mac = mac.upper()
    data = request.get_json()
    new_id = data.get("fixID", 0)
    if mac == OWN_MAC:
        global OWN_FIX_ID
        OWN_FIX_ID = new_id
        return jsonify({"status": "ok", "fixID": new_id})
    for p in get_peers():
        if p["mac"] == mac:
            udp_send(p["ip"], p["mac"], f"SETFIXID:{new_id}")
            p["fixID"] = new_id
            return jsonify({"status": "ok", "fixID": new_id})
    return jsonify({"status": "error", "message": "Peer not found"}), 404

@app.route("/api/heads/<mac>/name", methods=["POST"])
def api_set_name(mac):
    mac  = mac.upper()
    name = (request.get_json() or {}).get("name", "")
    with peers_lock:
        if mac in peers:
            peers[mac]["name"] = name
    # Update fixture pool entry for this MAC
    with fixtures_lock:
        for fx in fixtures.values():
            if fx.get("mac") == mac:
                fx["name"] = name
                break
    save_fixtures()
    # Forward name to the device via UDP
    for p in get_peers():
        if p["mac"] == mac:
            udp_send(p["ip"], p["mac"], f"SETNAME:{name}")
            break
    return jsonify({"status": "ok"})

@app.route("/api/fixtures")
def api_get_fixtures():
    return jsonify(get_fixtures_with_status())

@app.route("/api/fixtures", methods=["POST"])
def api_create_fixture():
    data = request.get_json() or {}
    fid  = int(data.get("id", 0))
    if fid <= 0:
        return jsonify({"status": "error", "message": "Invalid ID"}), 400
    with fixtures_lock:
        if fid in fixtures:
            return jsonify({"status": "error", "message": "ID already exists"}), 409
        fx = {"id": fid, "name": data.get("name", ""), "mac": data.get("mac") or None}
        fixtures[fid] = fx
    save_fixtures()
    return jsonify({"status": "ok", "fixture": fx})

@app.route("/api/fixtures/<int:fix_id>", methods=["PUT"])
def api_update_fixture(fix_id):
    data = request.get_json() or {}
    with fixtures_lock:
        if fix_id not in fixtures:
            return jsonify({"status": "error", "message": "Not found"}), 404
        if "name" in data:
            fixtures[fix_id]["name"] = data["name"]
        if "mac" in data:
            fixtures[fix_id]["mac"] = data["mac"] or None
    save_fixtures()
    return jsonify({"status": "ok"})

@app.route("/api/fixtures/<int:fix_id>", methods=["DELETE"])
def api_delete_fixture(fix_id):
    with fixtures_lock:
        if fix_id not in fixtures:
            return jsonify({"status": "error", "message": "Not found"}), 404
        del fixtures[fix_id]
    save_fixtures()
    return jsonify({"status": "ok"})

@app.route("/api/heads/<mac>/identify", methods=["POST"])
def api_identify(mac):
    mac  = mac.upper()
    data = request.get_json()
    on   = data.get("on", False)
    if mac == OWN_MAC or mac == "SELF":
        print(f"[Identify] Self: {'ON' if on else 'OFF'}")
        return jsonify({"status": "ok"})
    for p in get_peers():
        if p["mac"] == mac:
            udp_identify(p["ip"], p["mac"], on)
            return jsonify({"status": "ok"})
    return jsonify({"status": "error", "message": "Peer not found"}), 404

# ── /api/send ─────────────────────────────────────────────────────
@app.route("/api/send", methods=["POST"])
def api_send():
    data    = request.get_json()
    cmd     = data.get("command", "").strip()
    targets = data.get("targets", [])

    if not targets:
        print(f"[CMD] Self: {cmd}")
    else:
        for mac in targets:
            if mac == "*":
                print(f"[CMD] Broadcast: {cmd}")
                udp_broadcast(cmd)
                break
            elif mac in (OWN_MAC, "self"):
                print(f"[CMD] Self: {cmd}")
            else:
                for p in get_peers():
                    if p["mac"] == mac.upper():
                        udp_send(p["ip"], p["mac"], cmd)
                        break

    return jsonify({"status": "ok", "response": "OK"})

# ── /api/cues ─────────────────────────────────────────────────────
@app.route("/api/cues", methods=["GET"])
def api_get_cues():
    with cues_lock:
        return jsonify(cues)

@app.route("/api/cues", methods=["POST"])
def api_save_cue():
    data = request.get_json()
    raw_ft = data.get("fixTargets", [0])
    fix_targets = [int(x) for x in raw_ft] if raw_ft else [0]
    cue = {
        "id":          int(time.time() * 1000),
        "name":        data.get("name", "Cue"),
        "r":           max(0, min(255, int(data.get("r", 0)))),
        "g":           max(0, min(255, int(data.get("g", 0)))),
        "b":           max(0, min(255, int(data.get("b", 0)))),
        "w":           max(0, min(255, int(data.get("w", 0)))),
        "pan":         max(0, min(180, int(data.get("pan", 90)))),
        "tilt":        max(0, min(180, int(data.get("tilt", 90)))),
        "fixTargets":  fix_targets,
        "targetCount": len(fix_targets)
    }
    with cues_lock:
        cues.append(cue)
    save_cues()
    return jsonify({"status": "ok", "cue": cue})

@app.route("/api/cues/<int:cue_id>", methods=["DELETE"])
def api_delete_cue(cue_id):
    with cues_lock:
        idx = next((i for i, c in enumerate(cues) if c["id"] == cue_id), None)
        if idx is None:
            return jsonify({"status": "error", "message": "Not found"}), 404
        cues.pop(idx)
    save_cues()
    return jsonify({"status": "ok"})

@app.route("/api/cues/<int:cue_id>/fire", methods=["POST"])
def api_fire_cue(cue_id):
    with cues_lock:
        cue = next((c for c in cues if c["id"] == cue_id), None)
    if not cue:
        return jsonify({"status": "error", "message": "Not found"}), 404

    cmd = f"R:{cue['r']},G:{cue['g']},B:{cue['b']},W:{cue['w']},PAN:{cue['pan']},TILT:{cue['tilt']}"
    fix_targets = cue.get("fixTargets", [0])
    if not fix_targets:
        fix_targets = [0]

    for fid in fix_targets:
        if fid == 0:  # broadcast all
            udp_broadcast(cmd)
            print(f"[Fire] Broadcast: {cmd}")
            break
        else:
            for p in get_peers():
                if int(p.get("fixID", 0)) == fid:
                    udp_send(p["ip"], p["mac"], cmd)
                    print(f"[Fire] Fix#{fid} → {p['ip']}: {cmd}")
                    break

    return jsonify({"status": "ok", "command": "fired", "response": "OK"})

@app.route("/api/cues/reorder", methods=["PUT"])
def api_reorder_cues():
    data  = request.get_json() or {}
    order = [int(i) for i in data.get("order", [])]
    with cues_lock:
        by_id     = {c["id"]: c for c in cues}
        reordered = [by_id[i] for i in order if i in by_id]
        missing   = [c for c in cues if c["id"] not in set(order)]
        cues[:]   = reordered + missing
    save_cues()
    return jsonify({"status": "ok"})

@app.route("/api/cues/<int:cue_id>/targets", methods=["PUT"])
def api_update_targets(cue_id):
    data = request.get_json()
    with cues_lock:
        cue = next((c for c in cues if c["id"] == cue_id), None)
        if not cue:
            return jsonify({"status": "error", "message": "Not found"}), 404
        raw_ft = data.get("fixTargets", [])
        fix_targets = [int(x) for x in raw_ft] if raw_ft else [0]
        cue["fixTargets"]  = fix_targets
        cue["targetCount"] = len(fix_targets)
    save_cues()
    return jsonify({"status": "ok", "cue": cue})

# ── /api/sequencer ────────────────────────────────────────────────
seq_state = {
    "running":    False,
    "cue_ids":    [],
    "interval_ms": 1000,
    "loop":       True,
    "index":      0,
    "last_step":  0.0
}
seq_lock = threading.Lock()

def sequencer_runner():
    while True:
        with seq_lock:
            s = seq_state.copy()
        if s["running"] and s["cue_ids"]:
            now = time.time() * 1000
            if now - s["last_step"] >= s["interval_ms"]:
                tid = s["cue_ids"][s["index"]]
                with cues_lock:
                    cue = next((c for c in cues if c["id"] == tid), None)
                if cue:
                    cmd = f"R:{cue['r']},G:{cue['g']},B:{cue['b']},W:{cue['w']},PAN:{cue['pan']},TILT:{cue['tilt']}"
                    seq_ft = cue.get("fixTargets", [0]) or [0]
                    for fid in seq_ft:
                        if fid == 0:
                            udp_broadcast(cmd); break
                        else:
                            for p in get_peers():
                                if int(p.get("fixID", 0)) == fid:
                                    udp_send(p["ip"], p["mac"], cmd); break
                with seq_lock:
                    seq_state["last_step"] = now
                    seq_state["index"] += 1
                    if seq_state["index"] >= len(seq_state["cue_ids"]):
                        if seq_state["loop"]:
                            seq_state["index"] = 0
                        else:
                            seq_state["running"] = False
        time.sleep(0.05)

@app.route("/api/sequencer/start", methods=["POST"])
def api_seq_start():
    data = request.get_json()
    with seq_lock:
        seq_state["cue_ids"]     = [int(i) for i in data.get("cue_ids", [])]
        seq_state["interval_ms"] = int(data.get("interval_ms", 1000))
        seq_state["loop"]        = bool(data.get("loop", True))
        seq_state["index"]       = 0
        seq_state["last_step"]   = 0.0
        seq_state["running"]     = bool(seq_state["cue_ids"])
    return jsonify({"status": "ok"})

@app.route("/api/sequencer/stop", methods=["POST"])
def api_seq_stop():
    with seq_lock:
        seq_state["running"] = False
    return jsonify({"status": "ok"})

@app.route("/api/sequencer/status")
def api_seq_status():
    with seq_lock:
        return jsonify({"running": seq_state["running"]})

# ── /api/rainbow ─────────────────────────────────────────────────
@app.route("/api/rainbow", methods=["POST"])
def api_rainbow():
    data = request.get_json() or {}
    on   = bool(data.get("on", False))
    cmd  = "RAINBOW:1" if on else "RAINBOW:0"
    udp_broadcast(cmd)
    print(f"[Rainbow] Global {'ON' if on else 'OFF'} — sent to all peers")
    return jsonify({"status": "ok"})

# ── /api/artnet/patch ─────────────────────────────────────────────
@app.route("/api/artnet/patch")
def api_artnet_get():
    with artnet_patches_lock:
        return jsonify(list(artnet_patches))

@app.route("/api/artnet/patch", methods=["POST"])
def api_artnet_add():
    data = request.get_json() or {}
    fix_id     = int(data.get("fixID", 0))
    universe   = int(data.get("universe", 0))
    start_addr = int(data.get("startAddr", 1))
    if fix_id <= 0:
        return jsonify({"status": "error", "message": "Invalid fixID"}), 400
    if not (1 <= start_addr <= 512):
        return jsonify({"status": "error", "message": "startAddr out of range"}), 400
    with artnet_patches_lock:
        # Remove any existing patch for this fixID
        artnet_patches[:] = [p for p in artnet_patches if p["fixID"] != fix_id]
        artnet_patches.append({"fixID": fix_id, "universe": universe, "startAddr": start_addr})
        artnet_patches.sort(key=lambda p: (p["universe"], p["startAddr"]))
        save_artnet_patches()
    push_patch_to_esp(fix_id, universe, start_addr)
    return jsonify({"status": "ok"})

@app.route("/api/artnet/patch/<int:fix_id>", methods=["PUT"])
def api_artnet_update(fix_id):
    data = request.get_json() or {}
    with artnet_patches_lock:
        p = next((x for x in artnet_patches if x["fixID"] == fix_id), None)
        if not p:
            return jsonify({"status": "error", "message": "Not found"}), 404
        if "universe" in data:
            p["universe"]  = int(data["universe"])
        if "startAddr" in data:
            p["startAddr"] = int(data["startAddr"])
        save_artnet_patches()
        uni, addr = p["universe"], p["startAddr"]
    push_patch_to_esp(fix_id, uni, addr)
    return jsonify({"status": "ok"})

@app.route("/api/artnet/patch/<int:fix_id>", methods=["DELETE"])
def api_artnet_delete(fix_id):
    with artnet_patches_lock:
        before = len(artnet_patches)
        artnet_patches[:] = [p for p in artnet_patches if p["fixID"] != fix_id]
        if len(artnet_patches) == before:
            return jsonify({"status": "error", "message": "Not found"}), 404
        save_artnet_patches()
    return jsonify({"status": "ok"})

@app.route("/api/artnet/patch", methods=["DELETE"])
def api_artnet_clear_all():
    with artnet_patches_lock:
        removed = len(artnet_patches)
        artnet_patches.clear()
        save_artnet_patches()
    return jsonify({"status": "ok", "removed": removed})

@app.route("/api/artnet/patch/universe/<int:uni>", methods=["DELETE"])
def api_artnet_clear_universe(uni):
    with artnet_patches_lock:
        before = len(artnet_patches)
        artnet_patches[:] = [p for p in artnet_patches if p["universe"] != uni]
        removed = before - len(artnet_patches)
        if removed:
            save_artnet_patches()
    return jsonify({"status": "ok", "removed": removed})

@app.route("/api/artnet/patch/bulk", methods=["POST"])
def api_artnet_bulk():
    data       = request.get_json() or {}
    universe   = int(data.get("universe",   0))
    start_addr = int(data.get("startAddr",  1))
    count      = int(data.get("count",      1))
    first_fix  = int(data.get("firstFixID", 1))
    DMX_FP = 7
    if count < 1 or count > 32:
        return jsonify({"status": "error", "message": "count must be 1–32"}), 400
    if start_addr + count * DMX_FP - 1 > 512:
        return jsonify({"status": "error", "message": "Addresses exceed 512"}), 400
    bulk = []
    with artnet_patches_lock:
        for i in range(count):
            fix_id = first_fix + i
            addr   = start_addr + i * DMX_FP
            artnet_patches[:] = [p for p in artnet_patches if p["fixID"] != fix_id]
            artnet_patches.append({"fixID": fix_id, "universe": universe, "startAddr": addr})
            bulk.append((fix_id, universe, addr))
        artnet_patches.sort(key=lambda p: (p["universe"], p["startAddr"]))
        save_artnet_patches()
    for fix_id, uni, addr in bulk:
        push_patch_to_esp(fix_id, uni, addr)
    return jsonify({"status": "ok", "count": count})

@app.route("/api/artnet/status")
def api_artnet_status():
    return jsonify(get_artnet_status())

# ── Stub endpoints (for UI compatibility) ─────────────────────────
@app.route("/api/ports")
def api_ports():
    return jsonify([{"port": "PC", "description": f"PC Leader @ {OWN_IP}"}])

@app.route("/api/connect",    methods=["POST"])
def api_connect():    return jsonify({"status": "ok", "port": "PC"})

@app.route("/api/disconnect", methods=["POST"])
def api_disconnect(): return jsonify({"status": "ok"})

# ── Main ──────────────────────────────────────────────────────────
if __name__ == "__main__":
    load_cues()
    load_fixtures()
    load_artnet_patches()

    threading.Thread(target=beacon_sender,    daemon=True).start()
    threading.Thread(target=beacon_receiver,  daemon=True).start()
    threading.Thread(target=sequencer_runner, daemon=True).start()
    threading.Thread(target=artnet_sniffer,   daemon=True).start()

    print(f"[PC Leader] {APP_VERSION}")
    print(f"[PC Leader] MAC:  {OWN_MAC}")
    print(f"[PC Leader] IP:   {OWN_IP}")
    print(f"[PC Leader] Open: http://localhost:8080")
    print(f"[PC Leader] Beaconing on port {BEACON_PORT}, CMD on port {CMD_PORT}")

    app.run(host="0.0.0.0", port=8080, debug=False)
