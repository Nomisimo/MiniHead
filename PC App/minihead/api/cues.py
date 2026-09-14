"""Blueprint: /api/cues — cue storage and firing."""
from flask import Blueprint, jsonify
from .helpers import ok, err, require_json
from ..state.cues import CueStore
from ..state.peers import PeerStore
from ..state.fixtures import FixtureStore
from ..network.udp import send_udp_cmd, broadcast_udp_cmd
from .. import persistence, config


def create_cues_bp(
    cues: CueStore,
    peers: PeerStore,
    fixtures: FixtureStore,
    patches,
) -> Blueprint:
    bp = Blueprint("cues", __name__, url_prefix="/api/cues")

    @bp.get("")
    def list_cues():
        return jsonify(cues.to_list())

    @bp.post("")
    def create_cue():
        data = require_json()
        if data is None:
            return err("JSON body required")
        name = str(data.get("name", "")).strip()
        if not name:
            return err("name required")
        cue = cues.create(
            name=name,
            r=int(data.get("r", 0)),
            g=int(data.get("g", 0)),
            b=int(data.get("b", 0)),
            w=int(data.get("w", 0)),
            pan=int(data.get("pan", 90)),
            tilt=int(data.get("tilt", 45)),
            fix_targets=list(data.get("fixTargets", [0])),
        )
        persistence.save_cues(cues)
        return ok(id=cue.id, cue=cue.to_dict())

    @bp.delete("/<int:cid>")
    def delete_cue(cid: int):
        if not cues.delete(cid):
            return err("Not found", 404)
        persistence.save_cues(cues)
        return ok()

    @bp.post("/<int:cid>/fire")
    def fire_cue(cid: int):
        cue = cues.get(cid)
        if not cue:
            return err("Not found", 404)
        _fire(cue, peers, fixtures)
        return ok()

    @bp.put("/<int:cid>/targets")
    def update_targets(cid: int):
        data = require_json()
        if data is None:
            return err("JSON body required")
        if not cues.update_targets(cid, list(data.get("fixTargets", [0]))):
            return err("Not found", 404)
        persistence.save_cues(cues)
        return ok()

    @bp.put("/reorder")
    def reorder():
        data = require_json()
        if data is None:
            return err("JSON body required")
        order = [int(x) for x in data.get("order", [])]
        if not cues.reorder(order):
            return err("Count mismatch — some IDs missing")
        persistence.save_cues(cues)
        return ok()

    return bp


def _fire(cue, peers: PeerStore, fixtures: FixtureStore) -> None:
    cmd     = cue.dmx_command()
    targets = cue.fix_targets
    to_all  = not targets or targets == [0]

    all_peers = [(p.ip, p.mac) for p in peers.get_all() if p.mac != config.OWN_MAC]

    if to_all:
        broadcast_udp_cmd(all_peers, cmd)
        return

    for fid in targets:
        mac = fixtures.mac_for_fix_id(fid)
        if mac:
            ip = peers.get_ip(mac)
            if ip:
                send_udp_cmd(ip, mac, cmd)
