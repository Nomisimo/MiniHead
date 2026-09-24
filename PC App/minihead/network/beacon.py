"""BeaconService — UDP beacon sender + receiver threads."""
import socket
import threading
import time
import logging
from typing import Callable
from .. import config
from ..state.peers import PeerStore
from .net_utils import get_local_ip_for

log = logging.getLogger(__name__)

OWN_MAC = config.OWN_MAC


def _own_ip() -> str:
    ip = get_local_ip_for("8.8.8.8")
    return ip if ip else "127.0.0.1"


def own_ip() -> str:
    return _own_ip()


class BeaconService:
    """Manages UDP beacon send + receive and HTTP keepalive.

    on_new_peer(mac, fix_id) is called from the receiver thread whenever
    a previously-unknown peer appears — use it to push a stored patch.
    """

    def __init__(
        self,
        peers: PeerStore,
        on_new_peer: Callable[[str, int], None] | None = None,
    ) -> None:
        self._peers = peers
        self._on_new_peer = on_new_peer

    def start(self) -> None:
        for target in (self._sender, self._receiver, self._expiry_loop):
            t = threading.Thread(target=target, daemon=True, name=target.__name__)
            t.start()
            log.info("[init] thread: %s", t.name)

    # ── Sender ────────────────────────────────────────────────────────────────

    def _make_sender_sock(self, bind_ip: str) -> socket.socket:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        if bind_ip and bind_ip != "127.0.0.1":
            try:
                s.bind((bind_ip, 0))
            except Exception:
                pass
        return s

    def _sender(self) -> None:
        cur_ip = _own_ip()
        sock   = self._make_sender_sock(cur_ip)
        while True:
            try:
                ip = _own_ip()
                if ip != cur_ip:
                    sock.close()
                    cur_ip = ip
                    sock   = self._make_sender_sock(cur_ip)
                msg = f"MINIHEAD|{OWN_MAC}|{ip}|0|LEADER|PC|PC".encode()
                sock.sendto(msg, ("255.255.255.255", config.BEACON_PORT))
                # Unicast to each peer as backup (Telekom router blocks WiFi→WiFi broadcast)
                for peer in self._peers.get_all():
                    if peer.mac != OWN_MAC:
                        try:
                            sock.sendto(msg, (peer.ip, config.BEACON_PORT))
                        except Exception:
                            pass
                self._peers.touch(OWN_MAC)
            except Exception as e:
                log.warning("[beacon_sender] %s", e)
                sock.close()
                cur_ip = _own_ip()
                sock   = self._make_sender_sock(cur_ip)
            time.sleep(config.BEACON_INT)

    # ── Receiver ──────────────────────────────────────────────────────────────

    def _receiver(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        except AttributeError:
            pass
        sock.bind(("", config.BEACON_PORT))
        sock.settimeout(2.0)
        while True:
            try:
                data, _ = sock.recvfrom(256)
                text  = data.decode(errors="ignore").strip()
                parts = text.split("|")
                if len(parts) >= 5 and parts[0] == "MINIHEAD":
                    mac    = parts[1]
                    ip     = parts[2]
                    fix_id = int(parts[3]) if parts[3].isdigit() else 0
                    role   = parts[4]
                    name   = parts[5] if len(parts) > 5 else mac[-5:]
                    mode   = parts[6] if len(parts) > 6 else ""
                    if mac != OWN_MAC:
                        is_new = self._peers.upsert(mac, ip, fix_id, role, name, mode)
                        if is_new:
                            log.info("[beacon] new peer: %s %s fix=%d", mac, ip, fix_id)
                            if self._on_new_peer and fix_id > 0:
                                threading.Thread(
                                    target=self._on_new_peer,
                                    args=(mac, fix_id),
                                    daemon=True,
                                ).start()
            except socket.timeout:
                pass
            except Exception as e:
                log.warning("[beacon_receiver] %s", e)

    # ── Expiry loop ───────────────────────────────────────────────────────────

    def _expiry_loop(self) -> None:
        while True:
            time.sleep(config.EXPIRY_LOOP_INT)
            expired = self._peers.expire(config.PEER_TIMEOUT)
            for mac in expired:
                log.info("[beacon] expired: %s", mac)
