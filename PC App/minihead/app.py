"""Flask application factory."""
import os
import threading
import time
import logging
from flask import Flask, send_from_directory
from .state import PeerStore, FixtureStore, CueStore, PatchStore, SequencerEngine, ArtNetState
from .network.beacon import BeaconService
from .network.artnet_sniffer import ArtNetSniffer
from .network.esp import push_patch
from .api import (
    create_heads_bp, create_fixtures_bp, create_cues_bp,
    create_sequencer_bp, create_artnet_bp, create_commands_bp, create_system_bp,
)
from .api.cues import _fire
from . import persistence, config

log = logging.getLogger(__name__)


def create_app() -> Flask:
    app = Flask(__name__, static_folder=None)

    # ── Shared state ──────────────────────────────────────────────────────────
    peers     = PeerStore()
    fixtures  = FixtureStore()
    cues      = CueStore()
    patches   = PatchStore()
    sequencer = SequencerEngine()
    artnet    = ArtNetState()

    log_flags      = dict(config.DEFAULT_LOG_FLAGS)
    flags_lock     = threading.Lock()
    patch_acks     = {}
    patch_acks_lock = threading.Lock()

    # ── Load persisted data ───────────────────────────────────────────────────
    persistence.load_all(fixtures, patches, cues, log_flags)

    # ── Register self as the PC-leader peer ───────────────────────────────────
    from .network.beacon import own_ip
    peers.upsert(config.OWN_MAC, own_ip(), 0, "LEADER", "PC", "PC")

    # ── Blueprints ────────────────────────────────────────────────────────────
    app.register_blueprint(create_heads_bp(peers, patches, fixtures))
    app.register_blueprint(create_fixtures_bp(fixtures, peers, patches))
    app.register_blueprint(create_cues_bp(cues, peers, fixtures, patches))
    app.register_blueprint(create_sequencer_bp(sequencer))
    app.register_blueprint(create_artnet_bp(
        artnet, patches, peers, fixtures, patch_acks, patch_acks_lock,
    ))
    app.register_blueprint(create_commands_bp(peers))
    app.register_blueprint(create_system_bp(peers, log_flags, flags_lock))

    # ── Static files ──────────────────────────────────────────────────────────
    static_dir = config.STATIC_DIR

    @app.get("/")
    def index():
        return send_from_directory(static_dir, "index.html")

    @app.get("/css/<path:fname>")
    def css(fname):
        return send_from_directory(os.path.join(static_dir, "css"), fname)

    @app.get("/js/<path:fname>")
    def js(fname):
        return send_from_directory(os.path.join(static_dir, "js"), fname)

    # ── Background services ───────────────────────────────────────────────────
    def on_new_peer(mac: str, fix_id: int) -> None:
        """Called by BeaconService when a new device appears — push its stored patch."""
        patch = patches.get_by_fix_id(fix_id)
        if not patch:
            return
        peer = peers.get(mac)
        if peer:
            push_patch(peer.ip, mac, fix_id, patch.universe, patch.start_addr)

    beacon  = BeaconService(peers, on_new_peer=on_new_peer)
    sniffer = ArtNetSniffer(artnet, patches)
    beacon.start()
    sniffer.start()
    _start_sequencer_thread(sequencer, cues, peers, fixtures)
    _start_keepalive_thread(peers)

    return app


# ── Background worker helpers ─────────────────────────────────────────────────

def _start_sequencer_thread(sequencer: SequencerEngine, cues, peers, fixtures) -> None:
    def _run():
        while True:
            time.sleep(0.05)
            cue_id = sequencer.tick(time.time())
            if cue_id is None:
                continue
            cue = cues.get(cue_id)
            if cue:
                _fire(cue, peers, fixtures)
            else:
                log.warning("[seq] cue #%d not found", cue_id)

    t = threading.Thread(target=_run, daemon=True, name="sequencer_runner")
    t.start()
    log.info("[init] thread: sequencer_runner")


def _start_keepalive_thread(peers: PeerStore) -> None:
    from .network.esp import http_get

    def _run():
        while True:
            time.sleep(config.KEEPALIVE_INT)
            for p in peers.get_all():
                if p.mac == config.OWN_MAC:
                    continue
                result = http_get(p.ip, "/api/status")
                if result is not None:
                    peers.touch(p.mac)
                    log.debug("[keepalive] ok %s", p.mac)
                else:
                    log.debug("[keepalive] miss %s %s", p.mac, p.ip)

    t = threading.Thread(target=_run, daemon=True, name="http_keepalive")
    t.start()
    log.info("[init] thread: http_keepalive")
