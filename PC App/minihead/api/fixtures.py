"""Blueprint: /api/fixtures — offline fixture registry."""
from flask import Blueprint, jsonify
from .helpers import ok, err, require_json
from ..state.fixtures import FixtureStore
from ..state.peers import PeerStore
from .. import persistence


def create_fixtures_bp(fixtures: FixtureStore, peers: PeerStore, patches) -> Blueprint:
    bp = Blueprint("fixtures", __name__, url_prefix="/api/fixtures")

    @bp.get("")
    def list_fixtures():
        online_macs = {p.mac for p in peers.get_all()}
        return jsonify(fixtures.to_list(online_macs))

    @bp.post("")
    def create_fixture():
        data = require_json()
        if data is None:
            return err("JSON body required")
        fid = data.get("id")
        if not fid:
            return err("id required")
        fid  = int(fid)
        name = str(data.get("name", "")).strip()[:32] or f"Fix {fid}"
        mac  = data.get("mac") or None
        fixtures.upsert(fid, name, mac)
        persistence.save_data(fixtures, patches)
        return ok(id=fid)

    @bp.put("/<int:fid>")
    def update_fixture(fid: int):
        data = require_json()
        if data is None:
            return err("JSON body required")
        if not fixtures.update(fid, **{k: data[k] for k in ("name", "mac") if k in data}):
            return err("Not found", 404)
        persistence.save_data(fixtures, patches)
        return ok()

    @bp.delete("/<int:fid>")
    def delete_fixture(fid: int):
        fixtures.delete(fid)
        persistence.save_data(fixtures, patches)
        return ok()

    return bp
