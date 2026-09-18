"""Blueprint: /api/send + /api/control/* — command dispatch."""
from flask import Blueprint
from .helpers import ok, err, require_json
from ..state.peers import PeerStore
from ..network.udp import send_udp_cmd, broadcast_udp_cmd
from .. import config


def create_commands_bp(peers: PeerStore) -> Blueprint:
    bp = Blueprint("commands", __name__)

    def _all_esp_peers() -> list[tuple[str, str]]:
        return [(p.ip, p.mac) for p in peers.get_all() if p.mac != config.OWN_MAC]

    def _broadcast(cmd: str) -> None:
        broadcast_udp_cmd(_all_esp_peers(), cmd)

    def _send_to_macs(macs: list[str], cmd: str) -> None:
        for mac in macs:
            if mac == config.OWN_MAC:
                continue
            ip = peers.get_ip(mac)
            if ip:
                send_udp_cmd(ip, mac, cmd)

    @bp.post("/api/send")
    def send():
        data = require_json()
        if data is None:
            return err("JSON body required")
        cmd     = str(data.get("command", "")).strip()
        targets = list(data.get("targets", []))
        if not cmd:
            return err("command required")
        if targets:
            _send_to_macs(targets, cmd)
        else:
            _broadcast(cmd)
        return ok()

    @bp.post("/api/control/rainbow")
    def rainbow():
        data = require_json() or {}
        on   = bool(data.get("on", False))
        _broadcast("RAINBOW:1" if on else "RAINBOW:0")
        return ok(on=on)

    @bp.post("/api/control/demo")
    def demo():
        data = require_json() or {}
        on   = bool(data.get("on", False))
        _broadcast("DEMO:1" if on else "DEMO:0")
        return ok(on=on)

    @bp.post("/api/control/blackout")
    def blackout():
        _broadcast("BLACKOUT")
        return ok()

    @bp.post("/api/control/speed")
    def speed():
        data = require_json() or {}
        s = max(0.1, min(3.0, float(data.get("speed", 1.0))))
        _broadcast(f"SPEED:{s:.2f}")
        return ok(speed=s)

    # Legacy aliases — kept so existing ESP firmware UI still works
    @bp.post("/api/rainbow")
    def rainbow_legacy():
        return rainbow()

    @bp.post("/api/demo")
    def demo_legacy():
        return demo()

    @bp.post("/api/blackout")
    def blackout_legacy():
        return blackout()

    @bp.post("/api/animation/speed")
    def speed_legacy():
        return speed()

    return bp
