"""Blueprint: /api/heads — online device management."""
import threading
import time
from flask import Blueprint, jsonify
from .helpers import ok, err, require_json
from ..state.peers import PeerStore
from ..state.patches import PatchStore
from ..state.fixtures import FixtureStore
from ..network.udp import send_udp_raw
from ..network.esp import http_post
from .. import config


def create_heads_bp(
    peers: PeerStore,
    patches: PatchStore,
    fixtures: FixtureStore,
) -> Blueprint:
    bp = Blueprint("heads", __name__, url_prefix="/api/heads")

    @bp.get("")
    def list_heads():
        peers.expire(config.PEER_TIMEOUT)
        patch_list  = patches.get_all()
        peer_list   = peers.to_list(exclude_mac=config.OWN_MAC)

        result = [{
            "mac":       config.OWN_MAC,
            "ip":        _own_ip(),
            "fixID":     0,
            "name":      "PC",
            "role":      "LEADER",
            "mode":      "PC",
            "lastSeen":  time.time(),
            "online":    True,
            "universe":  None,
            "startAddr": None,
        }]

        for h in peer_list:
            fix_id = h["fixID"]
            patch  = next((p for p in patch_list if fix_id and p.fix_id == fix_id), None)
            h["universe"]  = patch.universe   if patch else None
            h["startAddr"] = patch.start_addr if patch else None
            result.append(h)

        return jsonify(result)

    @bp.post("/<mac>/fixid")
    def set_fix_id(mac: str):
        data = require_json()
        if data is None:
            return err("JSON body required")
        new_id = int(data.get("fixID", 0))
        peer = peers.get(mac)
        if peer:
            peers.upsert(peer.mac, peer.ip, new_id, peer.role, peer.name, peer.mode)
            # Push to ESP
            threading.Thread(
                target=http_post,
                args=(peer.ip, "/api/config/fixid", {"fixID": new_id}),
                daemon=True,
            ).start()
        return ok(fixID=new_id)

    @bp.post("/<mac>/name")
    def set_name(mac: str):
        data = require_json()
        if data is None:
            return err("JSON body required")
        name = str(data.get("name", ""))[:32]
        peer = peers.get(mac)
        if not peer:
            return err("Peer not found", 404)
        peers.upsert(peer.mac, peer.ip, peer.fix_id, peer.role, name, peer.mode)
        threading.Thread(
            target=http_post,
            args=(peer.ip, "/api/config/name", {"name": name}),
            daemon=True,
        ).start()
        return ok()

    @bp.post("/<mac>/identify")
    def identify(mac: str):
        data = require_json() or {}
        on   = bool(data.get("on", True))
        peer = peers.get(mac)
        if not peer:
            return err("Peer not found", 404)
        msg = f"IDENTIFY_ON|{mac}" if on else f"IDENTIFY_OFF|{mac}"
        send_udp_raw(peer.ip, msg)
        return ok()

    return bp


def _own_ip() -> str:
    from ..network.beacon import own_ip
    return own_ip()
