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
import logging
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
CUES_FILE      = "pc_cues.json"
DATA_FILE      = "pc_data.json"       # unified device data (replaces the three below)
LOG_FLAGS_FILE = "pc_log_flags.json"
# Legacy files — kept for one-time migration only; new installs use DATA_FILE
_LEGACY_FIXTURES_FILE     = "pc_fixtures.json"
_LEGACY_ARTNET_PATCH_FILE = "pc_artnet_patch.json"
_LEGACY_MAC_NAMES_FILE    = "pc_mac_names.json"

# ── PC Log configuration ──────────────────────────────────────────
# Must be defined early — module-level code below (UDP socket) uses pc_log().
_LOG_DEFAULTS = {
    "http":        False,   # Flask HTTP request lines (werkzeug)
    "udp":         True,    # UDP commands sent / received
    "discovery":   True,    # peer join / leave events
    "artnet":      True,    # ArtNet active / timeout / patch
    "fixtures":    False,   # fixture load / save events
    "sync":        False,   # patch sync / push-on-join details
}
_log_flags_lock = threading.Lock()
_log_flags = dict(_LOG_DEFAULTS)

def _load_log_flags():
    global _log_flags
    if os.path.exists(LOG_FLAGS_FILE):
        try:
            with open(LOG_FLAGS_FILE) as f:
                stored = json.load(f)
            with _log_flags_lock:
                _log_flags = {**_LOG_DEFAULTS, **stored}
        except Exception:
            pass

def _save_log_flags():
    with _log_flags_lock:
        data = dict(_log_flags)
    try:
        with open(LOG_FLAGS_FILE, "w") as f:
            json.dump(data, f, indent=2)
    except Exception:
        pass

def pc_log(category: str, msg: str):
    """Print only if the log flag for category is enabled."""
    with _log_flags_lock:
        enabled = _log_flags.get(category, True)
    if enabled:
        print(msg)

_load_log_flags()

# ── MAC → name mapping (persists names before a fixID is assigned) ─
_mac_names: dict = {}   # mac (upper) → name string

def _save_mac_names():
    save_data()   # delegates to the unified save

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
    resend_fid  = 0   # >0 means we need to re-send SETFIXID to the ESP
    use_fid     = 0

    with peers_lock:
        is_new      = mac not in peers
        # Re-push patches when a known peer was silent for >6 s (likely rebooted)
        # or came back on a different IP (DHCP change / genuine reconnect).
        was_silent  = (not is_new and
                       time.time() - peers[mac]["lastSeen"] > PEER_TIMEOUT * 0.5)
        ip_changed  = (not is_new and peers[mac]["ip"] != ip)
        should_push = is_new or was_silent or ip_changed

        beacon_fid = int(fix_id) if str(fix_id).isdigit() else 0

        # ── fixID arbitration ────────────────────────────────────────
        # For a continuously-online peer (not a fresh join / reconnect):
        # if the leader already assigned a different fixID, preserve it and
        # re-send the command — the beacon will eventually confirm the new ID.
        # For a fresh join / reconnect we trust the beacon (ESP knows its own NVS).
        if not is_new and not was_silent and not ip_changed:
            cur_fid = peers[mac].get("fixID", 0)
            if cur_fid > 0 and cur_fid != beacon_fid:
                use_fid    = cur_fid   # keep leader's assignment
                resend_fid = cur_fid   # schedule re-send
            else:
                use_fid = beacon_fid
        else:
            use_fid = beacon_fid

        if is_new:
            pc_log("discovery", f"[Discovery] Peer joined:       {mac}  IP:{ip}  Fix#{beacon_fid}  \"{name}\"  {role}")
        elif ip_changed:
            pc_log("discovery", f"[Discovery] Peer reconnected:  {mac}  new IP:{ip}  Fix#{beacon_fid}")
        elif was_silent:
            pc_log("discovery", f"[Discovery] Peer re-appeared:  {mac}  Fix#{beacon_fid}")

        peers[mac] = {
            "mac":      mac,
            "ip":       ip,
            "fixID":    use_fid,
            "name":     name,
            "role":     "LEADER" if role in ("LEADER", "1") else "FOLLOWER",
            "lastSeen": time.time()
        }

    # ── Beacon-based name confirmation ───────────────────────────────
    # If the beacon's name matches the last name we sent to this MAC,
    # the ESP has saved it — confirm the ACK even if NAMEACK was lost.
    intended_name = _mac_names.get(mac, "")
    if intended_name and name == intended_name:
        with _name_ack_lock:
            ack = _name_acks.get(mac)
            if ack and ack["status"] in ("pending", "timeout"):
                _name_acks[mac] = {"status": "ok", "ts": time.time()}
                pc_log("discovery", f"[Name] Confirmed via beacon: {mac}  \"{name}\"")

    # Re-send SETFIXID so ESP eventually adopts the leader-assigned value
    if resend_fid > 0:
        udp_send(ip, mac, f"SETFIXID:{resend_fid}")

    # Auto-register in fixture pool using the effective fixID (not the raw beacon value)
    if use_fid > 0:
        _fixture_auto_register(use_fid, name, mac)
    # Push stored fixID + patch on every new join / reconnect (works even if fid==0)
    if should_push:
        _push_config_on_join(beacon_fid, ip, mac)

def expire_peers():
    now = time.time()
    with peers_lock:
        expired = [m for m, p in peers.items() if now - p["lastSeen"] > PEER_TIMEOUT]
        for m in expired:
            pc_log("discovery", f"[Discovery] Peer expired: {m}")
            del peers[m]

def get_peers():
    with peers_lock:
        return list(peers.values())

# ── Fixture pool ──────────────────────────────────────────────────
fixtures_lock = threading.Lock()
fixtures = {}  # id(int) -> {id, name, mac}

def load_fixtures():
    pass   # handled by load_data() at startup

def _dedup_fixtures():
    """Remove duplicate MAC entries, keeping the lowest fixID per MAC.

    Run once at startup to repair fixtures corruated by the old buggy code
    that created a new entry on every fixID change instead of updating the
    existing one.
    """
    with fixtures_lock:
        # Group fixIDs by MAC (ignore entries with no MAC)
        mac_map = {}
        for fid, fx in fixtures.items():
            mac = fx.get("mac")
            if mac:
                mac_map.setdefault(mac, []).append(fid)

        removed = []
        for mac, fids in mac_map.items():
            if len(fids) > 1:
                keep = min(fids)   # lowest = most original assignment
                for fid in fids:
                    if fid != keep:
                        pc_log("fixtures", f"[Fixtures] Dedup: remove id={fid} "
                               f"(MAC={mac} → keeping Fix#{keep})")
                        removed.append(fid)

        for fid in removed:
            del fixtures[fid]
        if removed:
            save_fixtures()
            pc_log("fixtures", f"[Fixtures] Dedup: cleaned {len(removed)} duplicate(s)")

def save_fixtures():
    save_data()   # delegates to the unified save

def _fixture_auto_register(fid, name, mac):
    """Called when a peer beacons in with a fixID — keeps pool in sync.

    Ensures exactly ONE entry per MAC: stale entries for this MAC with a
    different fixID are removed before the current one is upserted.
    """
    with fixtures_lock:
        changed = False
        # Remove stale entries for this MAC with a different fixID
        if mac:
            stale = [k for k, fx in fixtures.items()
                     if fx.get("mac") == mac and k != fid]
            for k in stale:
                pc_log("fixtures", f"[Fixtures] Removing stale id={k} (MAC={mac} now Fix#{fid})")
                del fixtures[k]
                changed = True
        # Upsert current entry
        if fid not in fixtures:
            fixtures[fid] = {"id": fid, "name": name, "mac": mac}
            changed = True
        else:
            if not fixtures[fid].get("mac") and mac:
                fixtures[fid]["mac"] = mac;  changed = True
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
        pc_log("fixtures", f"[Cues] Loaded {len(cues)} cues")

def save_cues():
    with open(CUES_FILE, "w") as f:
        json.dump(cues, f, indent=2)

# ── Art-Net patch storage ─────────────────────────────────────────
# Each patch: {"fixID": int, "universe": int, "startAddr": int}
artnet_patches_lock = threading.Lock()
artnet_patches = []  # list of patch dicts

# ── Patch ACK tracking ────────────────────────────────────────────
_patch_ack_lock = threading.Lock()
_patch_acks = {}   # fixID(int) → {"status": "pending"|"ok"|"timeout", "ts": float}

def _patch_mark_pending(fix_id: int):
    with _patch_ack_lock:
        _patch_acks[int(fix_id)] = {"status": "pending", "ts": time.time()}

def _patch_mark_ok(fix_id: int):
    with _patch_ack_lock:
        _patch_acks[int(fix_id)] = {"status": "ok", "ts": time.time()}

# ── Name ACK tracking ─────────────────────────────────────────────
_name_ack_lock = threading.Lock()
_name_acks = {}    # mac(str) → {"status": "pending"|"ok"|"timeout", "ts": float}

def _name_mark_pending(mac: str):
    with _name_ack_lock:
        _name_acks[mac] = {"status": "pending", "ts": time.time()}

def _name_mark_ok(mac: str):
    with _name_ack_lock:
        _name_acks[mac] = {"status": "ok", "ts": time.time()}

def load_artnet_patches():
    pass   # handled by load_data() at startup

def save_artnet_patches():
    save_data()   # delegates to the unified save

# ── Unified data persistence (pc_data.json) ───────────────────────
# Single file replaces pc_fixtures.json + pc_artnet_patch.json + pc_mac_names.json.
# Schema: {"devices": [{mac, fixID, name, universe, startAddr}, ...]}
# universe/startAddr are null when no Art-Net patch exists for that device.

def save_data():
    """Write all device data (fixtures + patches + names) to pc_data.json."""
    # Snapshot each structure under its own lock to avoid holding multiple locks at once
    with artnet_patches_lock:
        patches_snap = [dict(p) for p in artnet_patches]
    with fixtures_lock:
        fixtures_snap = {k: dict(v) for k, v in fixtures.items()}
    mac_names_snap = dict(_mac_names)

    # Build unified list keyed by MAC
    mac_map = {}   # mac -> {mac, fixID, name, universe, startAddr}

    # Seed from _mac_names (names that may not have a fixture entry yet)
    for mac, name in mac_names_snap.items():
        mac_map.setdefault(mac, {"mac": mac, "fixID": 0, "name": "",
                                  "universe": None, "startAddr": None})
        if name:
            mac_map[mac]["name"] = name

    # Merge fixture pool entries
    no_mac_devices = []   # fixtures with no MAC (manually-added, MAC-less entries)
    for fid, fx in fixtures_snap.items():
        mac  = fx.get("mac")
        name = fx.get("name", "")
        if mac:
            e = mac_map.setdefault(mac, {"mac": mac, "fixID": 0, "name": "",
                                          "universe": None, "startAddr": None})
            e["fixID"] = fid
            if not e["name"] and name:
                e["name"] = name
        else:
            no_mac_devices.append({"mac": None, "fixID": fid, "name": name,
                                    "universe": None, "startAddr": None})

    # Merge Art-Net patches
    for p in patches_snap:
        fid  = p.get("fixID", 0)
        mac  = p.get("mac", "")
        uni  = p.get("universe")
        addr = p.get("startAddr")
        if mac:
            e = mac_map.setdefault(mac, {"mac": mac, "fixID": 0, "name": "",
                                          "universe": None, "startAddr": None})
            e["universe"]  = uni
            e["startAddr"] = addr
            if not e["fixID"] and fid:
                e["fixID"] = fid
        else:
            found = next((d for d in no_mac_devices if d["fixID"] == fid), None)
            if found:
                found["universe"]  = uni
                found["startAddr"] = addr
            else:
                no_mac_devices.append({"mac": None, "fixID": fid, "name": "",
                                        "universe": uni, "startAddr": addr})

    devices = list(mac_map.values()) + no_mac_devices
    try:
        with open(DATA_FILE, "w") as f:
            json.dump({"devices": devices}, f, indent=2)
    except Exception as e:
        print(f"[Data] Save error: {e}")


def _apply_data_dict(data: dict):
    """Populate in-memory structures from a unified data dict (no file I/O)."""
    global _mac_names, artnet_patches
    devices = data.get("devices", [])
    new_names    = {}
    new_patches  = []
    new_fixtures = {}
    for dev in devices:
        mac       = dev.get("mac")
        fid       = dev.get("fixID") or 0
        name      = dev.get("name", "")
        universe  = dev.get("universe")
        startAddr = dev.get("startAddr")
        if mac and name:
            new_names[mac] = name
        if fid > 0:
            new_fixtures[fid] = {"id": fid, "name": name, "mac": mac}
        if universe is not None and startAddr is not None:
            entry = {"fixID": fid, "universe": universe, "startAddr": startAddr}
            if mac:
                entry["mac"] = mac
            new_patches.append(entry)
    new_patches.sort(key=lambda p: (p["universe"], p["startAddr"]))
    _mac_names     = new_names
    artnet_patches = new_patches
    with fixtures_lock:
        fixtures.clear()
        fixtures.update(new_fixtures)
    _dedup_fixtures()
    pc_log("fixtures", f"[Data] Loaded {len(new_fixtures)} fixture(s), "
                       f"{len(new_patches)} patch(es), {len(new_names)} name(s)")


def load_data():
    """Load all device data; migrate from three legacy files if pc_data.json is absent."""
    if os.path.exists(DATA_FILE):
        try:
            with open(DATA_FILE) as f:
                raw = json.load(f)
            _apply_data_dict(raw)
            return
        except Exception as e:
            print(f"[Data] Error reading {DATA_FILE}: {e} — attempting migration")

    # ── One-time migration from three legacy files ────────────────────
    print("[Data] Migrating from legacy files…")
    mac_map      = {}
    no_mac_devs  = []

    if os.path.exists(_LEGACY_MAC_NAMES_FILE):
        try:
            with open(_LEGACY_MAC_NAMES_FILE) as f:
                for mac, name in json.load(f).items():
                    mac_map.setdefault(mac, {"mac": mac, "fixID": 0, "name": "",
                                              "universe": None, "startAddr": None})
                    if name:
                        mac_map[mac]["name"] = name
        except Exception:
            pass

    if os.path.exists(_LEGACY_FIXTURES_FILE):
        try:
            with open(_LEGACY_FIXTURES_FILE) as f:
                for fx in json.load(f):
                    mac  = fx.get("mac")
                    fid  = fx.get("id", 0)
                    name = fx.get("name", "")
                    if mac:
                        e = mac_map.setdefault(mac, {"mac": mac, "fixID": 0, "name": "",
                                                      "universe": None, "startAddr": None})
                        e["fixID"] = fid
                        if not e["name"] and name:
                            e["name"] = name
                    else:
                        no_mac_devs.append({"mac": None, "fixID": fid, "name": name,
                                             "universe": None, "startAddr": None})
        except Exception:
            pass

    if os.path.exists(_LEGACY_ARTNET_PATCH_FILE):
        try:
            with open(_LEGACY_ARTNET_PATCH_FILE) as f:
                for p in json.load(f):
                    mac  = p.get("mac", "")
                    fid  = p.get("fixID", 0)
                    uni  = p.get("universe")
                    addr = p.get("startAddr")
                    if mac:
                        e = mac_map.setdefault(mac, {"mac": mac, "fixID": 0, "name": "",
                                                      "universe": None, "startAddr": None})
                        e["universe"]  = uni
                        e["startAddr"] = addr
                        if not e["fixID"] and fid:
                            e["fixID"] = fid
                    else:
                        found = next((d for d in no_mac_devs if d["fixID"] == fid), None)
                        if found:
                            found["universe"]  = uni
                            found["startAddr"] = addr
                        else:
                            no_mac_devs.append({"mac": None, "fixID": fid, "name": "",
                                                 "universe": uni, "startAddr": addr})
        except Exception:
            pass

    merged = {"devices": list(mac_map.values()) + no_mac_devs}
    _apply_data_dict(merged)
    save_data()   # write the new unified file
    print("[Data] Migration complete → pc_data.json")


def _mac_for_fix(fix_id: int) -> str:
    """Return the MAC for a fixID from the fixture pool, or '' if unknown."""
    with fixtures_lock:
        fx = fixtures.get(int(fix_id))
        return fx.get("mac", "") if fx else ""

DMX_FOOTPRINT = 7   # channels per fixture (must match firmware CH_* layout)

def _patch_overlap_check(fix_id: int, universe: int, start_addr: int) -> int:
    """Return the fixID of the first conflicting patch, or 0 if clear.
    A conflict is any existing patch (other than fix_id itself) on the same universe
    whose 7-channel window overlaps [start_addr, start_addr+6].
    Must be called with artnet_patches_lock already held.
    """
    end = start_addr + DMX_FOOTPRINT - 1
    for p in artnet_patches:
        if p["fixID"] == fix_id:
            continue   # same fixture being replaced — skip
        if p["universe"] != universe:
            continue
        p_end = p["startAddr"] + DMX_FOOTPRINT - 1
        if start_addr <= p_end and end >= p["startAddr"]:
            return p["fixID"]
    return 0

def _patch_upsert(fix_id: int, universe: int, start_addr: int, mac: str = "") -> None:
    """Insert or replace the patch for this fixID (and MAC) — always exactly one entry per physical device.

    Deduplicates by fixID AND by MAC: if the same MAC already has a patch under a
    *different* fixID (e.g. after a fixID reassignment) that stale entry is removed too.
    Must be called with artnet_patches_lock already held.
    """
    artnet_patches[:] = [
        p for p in artnet_patches
        if p["fixID"] != fix_id and (not mac or p.get("mac") != mac)
    ]
    entry: dict = {"fixID": fix_id, "universe": universe, "startAddr": start_addr}
    if mac:
        entry["mac"] = mac
    artnet_patches.append(entry)
    artnet_patches.sort(key=lambda p: (p["universe"], p["startAddr"]))
    save_artnet_patches()

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
    pc_log("artnet", f"[ArtNet] Sniffer on UDP 6454 (broadcast only)")
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
            pc_log("artnet", f"[ArtNet] Sniffer error: {e}")

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
            pc_log("artnet", f"[ArtNet] Pushed SETPATCH:{fix_id},{universe},{start_addr} → {p['ip']}")
            _patch_mark_pending(fix_id)
            return
    pc_log("sync", f"[ArtNet] Fix#{fix_id} not online — patch stored locally only")

def _push_config_on_join(reported_fid: int, ip: str, mac: str):
    """When a peer joins/reconnects, push stored name + fixID + Art-Net patch so it is in sync.

    Works even when the ESP reports fixID=0 (fresh flash): we look up the MAC
    in the fixture pool and push the stored fixID, name, then the patch for that ID.
    """
    fid = reported_fid

    # 1. Check fixture pool for a stored fixID and name for this MAC
    with fixtures_lock:
        stored = next((fx for fx in fixtures.values() if fx.get("mac") == mac), None)
        stored_fid  = stored["id"]           if stored else 0
        stored_name = stored.get("name", "") if stored else ""
    # Also check mac_names for a name saved before a fixID was assigned
    if not stored_name:
        stored_name = _mac_names.get(mac, "")

    # 2. If fixture pool has a different fixID than ESP reports → push SETFIXID
    if stored_fid > 0 and stored_fid != reported_fid:
        with peers_lock:
            if mac in peers:
                peers[mac]["fixID"] = stored_fid   # keep local table consistent
        udp_send(ip, mac, f"SETFIXID:{stored_fid}")
        pc_log("sync", f"[Sync] Push SETFIXID:{stored_fid} → {ip}")
        fid = stored_fid

    # 3. Push stored name so the ESP always has the PC-assigned label.
    #    No _name_mark_pending here — this is an automatic sync push, not a user edit.
    #    The ✓/⟳/✗ ACK indicator is only set from api_set_name (user action).
    if stored_name:
        udp_send(ip, mac, f"SETNAME:{stored_name}")
        pc_log("sync", f"[Sync] Push SETNAME:{stored_name} → {ip}")

    # 4. Push Art-Net patch for whichever fixID we settled on.
    #    No _patch_mark_pending here — same reason as above.
    if fid > 0:
        with artnet_patches_lock:
            patch = next((p for p in artnet_patches if p["fixID"] == fid), None)
        if patch:
            udp_send(ip, mac, f"SETPATCH:{fid},{patch['universe']},{patch['startAddr']}")
            pc_log("sync", f"[Sync] Push SETPATCH:{fid},{patch['universe']},{patch['startAddr']} → {ip}")

# ── UDP beacon sender ─────────────────────────────────────────────
def beacon_sender():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    while True:
        pkt = f"MINIHEAD|{OWN_MAC}|{OWN_IP}|{OWN_FIX_ID}|LEADER|PC"
        try:
            sock.sendto(pkt.encode(), (BROADCAST_ADDR, BEACON_PORT))
        except Exception as e:
            pc_log("udp", f"[Beacon] Send error: {e}")
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
            if msg.startswith("PATCHACK|"):
                parts = msg.split("|")
                if len(parts) >= 3:
                    try:
                        _patch_mark_ok(int(parts[2]))
                        pc_log("artnet", f"[ArtNet] PATCHACK Fix#{parts[2]} from {parts[1]}")
                    except ValueError:
                        pass
                continue
            if msg.startswith("NAMEACK|"):
                parts = msg.split("|")
                if len(parts) >= 2:
                    _name_mark_ok(parts[1].upper())
                    pc_log("discovery", f"[Name] NAMEACK from {parts[1]}")
                continue
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
            pc_log("discovery", f"[Beacon RX] Error: {e}")

# ── UDP command sender ────────────────────────────────────────────
# Do NOT bind to a specific IP — let the OS route freely based on the
# destination address, exactly like the beacon_sender socket does.
# Binding to OWN_IP locks the socket to one interface (e.g. Ethernet)
# and can prevent packets from reaching WiFi-only devices when the
# Fritz!Box doesn't forward subnet broadcasts across the LAN/WiFi bridge.
_cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
_cmd_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
pc_log("udp", f"[UDP] Cmd socket ready (unbound — OS picks interface per destination)")

def udp_send(ip, target_mac, command):
    pkt = f"CMD|{target_mac}|{command}".encode()
    sent = False
    # 1) Directed subnet broadcast — reaches all devices including WiFi clients
    #    even when the PC is on Ethernet (Fritz!Box forwards subnet broadcasts).
    try:
        _cmd_sock.sendto(pkt, (BROADCAST_ADDR, CMD_PORT))
        sent = True
    except Exception as e:
        pc_log("udp", f"[UDP] Broadcast error ({BROADCAST_ADDR}:{CMD_PORT}): {e}")
    # 2) Unicast fallback — after receiving a beacon from the ESP the ARP entry
    #    is cached, so unicast resolves correctly even across WiFi/Ethernet boundary.
    #    Duplicate delivery is harmless: ESP overwrites NVS with the same values.
    if ip and ip not in ("", "0.0.0.0", BROADCAST_ADDR):
        try:
            _cmd_sock.sendto(pkt, (ip, CMD_PORT))
            sent = True
        except Exception:
            pass   # silently ignore unicast failures — broadcast is the primary path
    if sent:
        pc_log("udp", f"[UDP] → {target_mac}  {command}")
    else:
        pc_log("udp", f"[UDP] FAILED → {target_mac}  {command}")

def udp_identify(ip, target_mac, on: bool):
    kind = "IDENTIFY_ON" if on else "IDENTIFY_OFF"
    pkt  = f"{kind}|{target_mac}".encode()
    try:
        _cmd_sock.sendto(pkt, (BROADCAST_ADDR, CMD_PORT))
    except Exception as e:
        pc_log("udp", f"[UDP] Identify error (bcast {BROADCAST_ADDR}): {e}")
    # Unicast fallback — same dual-send strategy as udp_send
    if ip and ip not in ("", "0.0.0.0", BROADCAST_ADDR):
        try:
            _cmd_sock.sendto(pkt, (ip, CMD_PORT))
        except Exception:
            pass

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

@app.route("/plugins/log/log_panel.html")
def serve_log_panel():
    return send_from_directory("plugins/log", "log_panel.html")

# ── /api/logconfig  (PC log flags) ───────────────────────────────
@app.route("/api/logconfig", methods=["GET"])
def api_get_logconfig():
    with _log_flags_lock:
        return jsonify(dict(_log_flags))

@app.route("/api/logconfig", methods=["POST"])
def api_set_logconfig():
    data = request.get_json(silent=True) or {}
    with _log_flags_lock:
        for k, v in data.items():
            if k in _log_flags:
                _log_flags[k] = bool(v)
    _save_log_flags()
    # Apply werkzeug HTTP log level immediately
    with _log_flags_lock:
        http_on = _log_flags.get("http", False)
    logging.getLogger("werkzeug").setLevel(
        logging.DEBUG if http_on else logging.WARNING
    )
    with _log_flags_lock:
        return jsonify(dict(_log_flags))

# ── /api/esp/logconfig  (proxy to ESP leader) ────────────────────
@app.route("/api/esp/logconfig", methods=["GET", "POST"])
def api_esp_logconfig():
    """Forward log config GET/POST to the currently-known ESP leader."""
    # Find any online ESP peer (they all run the same leader when PC is leader,
    # so we can send to any of them — or pick fixID-sorted first).
    with peers_lock:
        targets = [p for p in peers.values()]
    if not targets:
        return jsonify({"status": "error", "message": "No ESP peers online"}), 503
    target = sorted(targets, key=lambda p: p.get("fixID", 999))[0]
    esp_url = f"http://{target['ip']}/api/logconfig"
    try:
        if request.method == "GET":
            with urllib.request.urlopen(esp_url, timeout=3) as r:
                body = r.read()
            return body, 200, {"Content-Type": "application/json"}
        else:
            body = request.get_data()
            req2 = urllib.request.Request(
                esp_url, data=body,
                headers={"Content-Type": "application/json"},
                method="POST"
            )
            with urllib.request.urlopen(req2, timeout=3) as r:
                resp_body = r.read()
            return resp_body, 200, {"Content-Type": "application/json"}
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 502

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
    new_id = int(data.get("fixID", 0))
    if mac == OWN_MAC:
        global OWN_FIX_ID
        OWN_FIX_ID = new_id
        return jsonify({"status": "ok", "fixID": new_id})
    # Update the actual peers dict (not a copy) so /api/heads returns the new value immediately
    with peers_lock:
        if mac not in peers:
            return jsonify({"status": "error", "message": "Peer not found"}), 404
        peers[mac]["fixID"] = new_id
        ip = peers[mac]["ip"]
    udp_send(ip, mac, f"SETFIXID:{new_id}")
    pc_log("udp", f"[FixID] Sent SETFIXID:{new_id} → {ip}")

    # Update fixture pool: remove stale entries for this MAC, upsert with new ID
    with fixtures_lock:
        peer_name = ""
        with peers_lock:
            if mac in peers:
                peer_name = peers[mac].get("name", "")
        stale = [k for k, fx in fixtures.items()
                 if fx.get("mac") == mac and k != new_id]
        for k in stale:
            pc_log("fixtures", f"[FixID] Removing stale fixture id={k} for MAC={mac}")
            del fixtures[k]
        if new_id > 0:
            if new_id not in fixtures:
                fixtures[new_id] = {"id": new_id, "name": peer_name, "mac": mac}
            else:
                if not fixtures[new_id].get("mac"):
                    fixtures[new_id]["mac"] = mac
        save_fixtures()

    # Migrate / clean artnet patches: remove stale fixID entries for this MAC,
    # migrating the patch to the new fixID if the new one doesn't have one yet.
    # NOTE: use `stale` (computed before fixture deletion above), not fixtures.items()
    if new_id > 0:
        with artnet_patches_lock:
            stale_fids = set(stale)   # old fixIDs for this MAC, already removed from fixture pool
            # Grab the old patch (if any) before removing stale entries
            old_patch = next(
                (p for p in artnet_patches if p.get("fixID") in stale_fids), None
            )
            new_patch = next(
                (p for p in artnet_patches if p["fixID"] == new_id), None
            )
            # Remove stale entries
            artnet_patches[:] = [p for p in artnet_patches
                                  if p.get("fixID") not in stale_fids]
            # Migrate address to new ID if new ID has no patch yet
            if old_patch and not new_patch:
                migrated = {"fixID": new_id, "universe": old_patch["universe"],
                            "startAddr": old_patch["startAddr"], "mac": mac}
                artnet_patches.append(migrated)
                artnet_patches.sort(key=lambda p: (p["universe"], p["startAddr"]))
                pc_log("artnet", f"[ArtNet] Migrated patch fixID {old_patch['fixID']}→{new_id}"
                       f"  U{migrated['universe']}@{migrated['startAddr']}")
            # Ensure MAC is tagged on existing new patch
            if new_patch:
                new_patch["mac"] = mac
            save_artnet_patches()

        # Push whichever patch the new fixID now has
        with artnet_patches_lock:
            patch = next((p for p in artnet_patches if p["fixID"] == new_id), None)
        if patch:
            udp_send(ip, mac, f"SETPATCH:{new_id},{patch['universe']},{patch['startAddr']}")
            pc_log("udp", f"[FixID] Pushed SETPATCH:{new_id},{patch['universe']},{patch['startAddr']} → {ip}")

    return jsonify({"status": "ok", "fixID": new_id})

@app.route("/api/heads/<mac>/name", methods=["POST"])
def api_set_name(mac):
    mac  = mac.upper()
    name = (request.get_json() or {}).get("name", "")
    with peers_lock:
        if mac in peers:
            peers[mac]["name"] = name
    # Always persist the name keyed by MAC so it survives fixID=0 and re-joins
    _mac_names[mac] = name
    _save_mac_names()
    # Also update fixture pool entry for this MAC (if one exists)
    with fixtures_lock:
        found = False
        for fx in fixtures.values():
            if fx.get("mac") == mac:
                fx["name"] = name
                found = True
                break
        if not found:
            # No fixture entry yet — create one using the peer's current fixID
            with peers_lock:
                peer_fid = peers.get(mac, {}).get("fixID", 0)
            if peer_fid > 0:
                if peer_fid not in fixtures:
                    fixtures[peer_fid] = {"id": peer_fid, "name": name, "mac": mac}
                else:
                    fixtures[peer_fid]["mac"]  = mac
                    fixtures[peer_fid]["name"] = name
    save_fixtures()
    # Forward name to the device via UDP
    for p in get_peers():
        if p["mac"] == mac:
            udp_send(p["ip"], p["mac"], f"SETNAME:{name}")
            _name_mark_pending(mac)
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
        pc_log("udp", f"[Identify] Self: {'ON' if on else 'OFF'}")
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
        pc_log("udp", f"[CMD] Self: {cmd}")
    else:
        for mac in targets:
            if mac == "*":
                pc_log("udp", f"[CMD] Broadcast: {cmd}")
                udp_broadcast(cmd)
                break
            elif mac in (OWN_MAC, "self"):
                pc_log("udp", f"[CMD] Self: {cmd}")
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
            pc_log("udp", f"[Fire] Broadcast: {cmd}")
            break
        else:
            for p in get_peers():
                if int(p.get("fixID", 0)) == fid:
                    udp_send(p["ip"], p["mac"], cmd)
                    pc_log("udp", f"[Fire] Fix#{fid} → {p['ip']}: {cmd}")
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
    pc_log("udp", f"[Rainbow] Global {'ON' if on else 'OFF'} — sent to all peers")
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
    if start_addr + DMX_FOOTPRINT - 1 > 512:
        return jsonify({"status": "error", "message": "Address window exceeds channel 512"}), 400
    mac = _mac_for_fix(fix_id)
    with artnet_patches_lock:
        conflict = _patch_overlap_check(fix_id, universe, start_addr)
        if conflict:
            return jsonify({"status": "error",
                            "message": f"Overlap with Fix#{conflict} (channels {start_addr}–{start_addr+DMX_FOOTPRINT-1})"}), 409
        _patch_upsert(fix_id, universe, start_addr, mac)
    push_patch_to_esp(fix_id, universe, start_addr)
    return jsonify({"status": "ok"})

@app.route("/api/artnet/patch/<int:fix_id>", methods=["PUT"])
def api_artnet_update(fix_id):
    data = request.get_json() or {}
    with artnet_patches_lock:
        p = next((x for x in artnet_patches if x["fixID"] == fix_id), None)
        if not p:
            return jsonify({"status": "error", "message": "Not found"}), 404
        new_uni  = int(data["universe"])  if "universe"  in data else p["universe"]
        new_addr = int(data["startAddr"]) if "startAddr" in data else p["startAddr"]
        if not (1 <= new_addr <= 512) or new_addr + DMX_FOOTPRINT - 1 > 512:
            return jsonify({"status": "error", "message": "startAddr out of range"}), 400
        conflict = _patch_overlap_check(fix_id, new_uni, new_addr)
        if conflict:
            return jsonify({"status": "error",
                            "message": f"Overlap with Fix#{conflict} (channels {new_addr}–{new_addr+DMX_FOOTPRINT-1})"}), 409
        p["universe"]  = new_uni
        p["startAddr"] = new_addr
        # Keep MAC tag up to date
        if not p.get("mac"):
            p["mac"] = _mac_for_fix(fix_id)
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

@app.route("/api/artnet/patch/ack")
def api_patch_ack():
    now = time.time()
    result = {}
    with _patch_ack_lock:
        for fid, ack in _patch_acks.items():
            if ack["status"] == "pending" and now - ack["ts"] > 8.0:
                ack["status"] = "timeout"
            result[str(fid)] = dict(ack)
    return jsonify(result)

@app.route("/api/heads/ack")
def api_name_ack():
    now = time.time()
    result = {}
    with _name_ack_lock:
        for mac, ack in _name_acks.items():
            if ack["status"] == "pending" and now - ack["ts"] > 8.0:
                ack["status"] = "timeout"
            result[mac] = dict(ack)
    return jsonify(result)

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
        # Pre-check all addresses for conflicts before committing any
        for i in range(count):
            fix_id = first_fix + i
            addr   = start_addr + i * DMX_FP
            conflict = _patch_overlap_check(fix_id, universe, addr)
            if conflict:
                return jsonify({"status": "error",
                                "message": f"Fix#{fix_id} addr {addr} overlaps Fix#{conflict}"}), 409
        for i in range(count):
            fix_id = first_fix + i
            addr   = start_addr + i * DMX_FP
            mac    = _mac_for_fix(fix_id)
            _patch_upsert(fix_id, universe, addr, mac)
            bulk.append((fix_id, universe, addr))
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
    # Suppress Flask/werkzeug HTTP request logs unless flag is enabled
    with _log_flags_lock:
        http_on = _log_flags.get("http", False)
    logging.getLogger("werkzeug").setLevel(
        logging.DEBUG if http_on else logging.WARNING
    )

    load_cues()
    load_data()   # unified: replaces load_fixtures() + load_artnet_patches() + _load_mac_names()

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
