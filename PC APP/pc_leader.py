#!/usr/bin/env python3
"""
pc_leader.py — MiniHead PC Leader
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
from flask import Flask, request, jsonify, send_from_directory

# ── Config ────────────────────────────────────────────────────────
BEACON_PORT   = 4210
CMD_PORT      = 4211
BEACON_INTERVAL = 1.0       # seconds
PEER_TIMEOUT    = 12.0      # seconds before peer is removed (ESP32 beacons every 2s)
OWN_FIX_ID      = 0         # set your fixture ID here
CUES_FILE       = "pc_cues.json"
FIXTURES_FILE   = "pc_fixtures.json"

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

OWN_MAC  = get_own_mac()
OWN_IP   = get_own_ip()
OWN_ROLE = "LEADER"

# ── Peer table ────────────────────────────────────────────────────
peers_lock = threading.Lock()
peers = {}  # mac -> {mac, ip, fixID, name, role, lastSeen}

def update_peer(mac, ip, fix_id, role, name=""):
    with peers_lock:
        is_new = mac not in peers
        peers[mac] = {
            "mac":      mac,
            "ip":       ip,
            "fixID":    int(fix_id) if str(fix_id).isdigit() else 0,
            "name":     name,
            "role":     "LEADER" if role in ("LEADER", "1") else "FOLLOWER",
            "lastSeen": time.time()
        }
        if is_new:
            print(f"[Discovery] Peer joined: {mac}  IP:{ip}  Fix#{fix_id}  \"{name}\"  {role}")
    # Auto-register in fixture pool if fixID assigned
    fid = int(fix_id) if str(fix_id).isdigit() else 0
    if fid > 0:
        _fixture_auto_register(fid, name, mac)

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

# ── UDP beacon sender ─────────────────────────────────────────────
def beacon_sender():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    while True:
        pkt = f"MINIHEAD|{OWN_MAC}|{OWN_IP}|{OWN_FIX_ID}|LEADER|PC"
        try:
            sock.sendto(pkt.encode(), ("<broadcast>", BEACON_PORT))
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

    threading.Thread(target=beacon_sender,   daemon=True).start()
    threading.Thread(target=beacon_receiver, daemon=True).start()
    threading.Thread(target=sequencer_runner, daemon=True).start()

    print(f"[PC Leader] MAC:  {OWN_MAC}")
    print(f"[PC Leader] IP:   {OWN_IP}")
    print(f"[PC Leader] Open: http://localhost:5000")
    print(f"[PC Leader] Beaconing on port {BEACON_PORT}, CMD on port {CMD_PORT}")

    app.run(host="0.0.0.0", port=8080, debug=False)
