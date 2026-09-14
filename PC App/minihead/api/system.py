"""Blueprint: /api/status, /api/version, /api/logconfig."""
import threading
from flask import Blueprint, jsonify
from .helpers import ok, err, require_json
from ..state.peers import PeerStore
from ..network.esp import push_log_flags
from .. import config, persistence


def create_system_bp(peers: PeerStore, log_flags: dict, flags_lock) -> Blueprint:
    bp = Blueprint("system", __name__, url_prefix="/api")

    @bp.get("/status")
    def status():
        from ..network.beacon import own_ip
        return jsonify({"status": "ok", "ip": own_ip(), "role": "LEADER",
                        "version": config.APP_VERSION})

    @bp.get("/version")
    def version():
        return jsonify({"version": config.APP_VERSION})

    @bp.get("/logconfig")
    def get_logconfig():
        with flags_lock:
            return jsonify(dict(log_flags))

    @bp.post("/logconfig")
    def set_logconfig():
        data = require_json()
        if data is None:
            return err("JSON body required")
        with flags_lock:
            for k in log_flags:
                if k in data:
                    log_flags[k] = bool(data[k])
        persistence.save_flags(log_flags)
        return ok()

    @bp.post("/logconfig/push")
    def push_logconfig():
        """Update flags AND push to all connected ESPs."""
        data = require_json()
        if data is None:
            return err("JSON body required")
        with flags_lock:
            for k in log_flags:
                if k in data:
                    log_flags[k] = bool(data[k])
            flags_copy = dict(log_flags)
        persistence.save_flags(log_flags)

        def _push():
            for p in peers.get_all():
                if p.mac != config.OWN_MAC:
                    push_log_flags(p.ip, p.mac, flags_copy)

        threading.Thread(target=_push, daemon=True).start()
        return ok()

    # Legacy alias
    @bp.get("/esp/logconfig")
    def get_esp_logconfig():
        return get_logconfig()

    @bp.post("/esp/logconfig")
    def set_esp_logconfig():
        return push_logconfig()

    return bp
