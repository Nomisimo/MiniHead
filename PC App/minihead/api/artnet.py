"""Blueprint: /api/artnet — patch management and status."""
import threading
from flask import Blueprint, jsonify
from .helpers import ok, err, require_json
from ..state.artnet import ArtNetState
from ..state.patches import PatchStore
from ..state.peers import PeerStore
from ..network.esp import push_patch, delete_patch, poll_patches
from ..models import Patch
from .. import persistence, config


def create_artnet_bp(
    artnet: ArtNetState,
    patches: PatchStore,
    peers: PeerStore,
    fixtures,
    patch_acks: dict,
    patch_acks_lock,
) -> Blueprint:
    bp = Blueprint("artnet", __name__, url_prefix="/api/artnet")

    @bp.get("/status")
    def status():
        return jsonify(artnet.status(len(patches.get_all())))

    @bp.get("/patch")
    def list_patches():
        return jsonify(patches.to_list())

    @bp.get("/patch/ack")
    def patch_acks_endpoint():
        with patch_acks_lock:
            return jsonify({str(k): v for k, v in patch_acks.items()})

    @bp.post("/patch")
    def create_patch():
        data = require_json()
        if data is None:
            return err("JSON body required")
        fid  = int(data.get("fixID", 0))
        uni  = int(data.get("universe", 0))
        addr = int(data.get("startAddr", 1))
        patches.upsert(fid, uni, addr)
        persistence.save_data(fixtures, patches)
        threading.Thread(
            target=_push_to_esp, args=(fid, uni, addr, peers, patch_acks, patch_acks_lock),
            daemon=True,
        ).start()
        return ok()

    @bp.put("/patch/<int:fid>")
    def update_patch(fid: int):
        data = require_json()
        if data is None:
            return err("JSON body required")
        uni  = int(data["universe"])  if "universe"  in data else None
        addr = int(data["startAddr"]) if "startAddr" in data else None
        patch = patches.update(fid, universe=uni, start_addr=addr)
        if not patch:
            return err("Not found", 404)
        persistence.save_data(fixtures, patches)
        threading.Thread(
            target=_push_to_esp,
            args=(fid, patch.universe, patch.start_addr, peers, patch_acks, patch_acks_lock),
            daemon=True,
        ).start()
        return ok()

    @bp.delete("/patch/<int:fid>")
    def delete_patch_route(fid: int):
        patches.delete(fid)
        persistence.save_data(fixtures, patches)
        threading.Thread(
            target=_delete_from_esps, args=(fid, peers),
            daemon=True,
        ).start()
        return ok()

    @bp.delete("/patch")
    def clear_patches():
        patches.clear()
        persistence.save_data(fixtures, patches)
        return ok()

    @bp.post("/patch/bulk")
    def bulk_patches():
        data = require_json()
        if data is None:
            return err("JSON body required")
        new_patches = [
            Patch(fix_id=int(d.get("fixID", 0)),
                  universe=int(d.get("universe", 0)),
                  start_addr=int(d.get("startAddr", 1)))
            for d in data.get("patches", [])
            if int(d.get("fixID", 0)) > 0
        ]
        patches.bulk_set(new_patches)
        persistence.save_data(fixtures, patches)
        for p in new_patches:
            threading.Thread(
                target=_push_to_esp,
                args=(p.fix_id, p.universe, p.start_addr, peers, patch_acks, patch_acks_lock),
                daemon=True,
            ).start()
        return ok(count=len(new_patches))

    @bp.post("/poll")
    def poll():
        nodes = _poll_all_esps(peers)
        for node in nodes:
            fid, uni, addr = node["fixID"], node["universe"], node["startAddr"]
            if fid > 0:
                patches.upsert(fid, uni, addr)
        persistence.save_data(fixtures, patches)
        return ok(nodes=nodes)

    return bp


# ── Helpers ───────────────────────────────────────────────────────────────────

def _push_to_esp(fid, uni, addr, peers, acks, lock):
    targets = [(p.ip, p.mac) for p in peers.get_all()
               if p.fix_id == fid and p.mac != config.OWN_MAC]
    if not targets:
        return
    with lock:
        acks[fid] = {"status": "pending"}
    success = any(push_patch(ip, mac, fid, uni, addr) for ip, mac in targets)
    with lock:
        acks[fid] = {"status": "ok" if success else "timeout"}


def _delete_from_esps(fid, peers):
    for p in peers.get_all():
        if p.fix_id == fid and p.mac != config.OWN_MAC:
            delete_patch(p.ip, p.mac, fid)


def _poll_all_esps(peers) -> list[dict]:
    nodes = []
    for p in peers.get_all():
        if p.mac == config.OWN_MAC:
            continue
        nodes.extend(poll_patches(p.ip, p.fix_id, p.name))
    return nodes
