import threading
import time
from typing import Optional
from ..models import Peer


class PeerStore:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._peers: dict[str, Peer] = {}

    def upsert(self, mac: str, ip: str, fix_id: int, role: str,
               name: str, mode: str = "") -> bool:
        """Update or insert a peer. Returns True if this is a new peer."""
        with self._lock:
            is_new = mac not in self._peers
            self._peers[mac] = Peer(
                mac=mac, ip=ip, fix_id=fix_id,
                name=name, role=role, mode=mode,
                last_seen=time.time(),
            )
            return is_new

    def touch(self, mac: str) -> None:
        with self._lock:
            if mac in self._peers:
                self._peers[mac].last_seen = time.time()

    def get(self, mac: str) -> Optional[Peer]:
        with self._lock:
            return self._peers.get(mac)

    def get_all(self) -> list[Peer]:
        with self._lock:
            return list(self._peers.values())

    def get_ip(self, mac: str) -> Optional[str]:
        with self._lock:
            p = self._peers.get(mac)
            return p.ip if p else None

    def expire(self, timeout: float) -> list[str]:
        """Remove peers not seen within timeout seconds. Returns expired MACs."""
        cutoff = time.time() - timeout
        with self._lock:
            expired = [m for m, p in self._peers.items() if p.last_seen < cutoff]
            for m in expired:
                del self._peers[m]
        return expired

    def to_list(self, exclude_mac: Optional[str] = None) -> list[dict]:
        with self._lock:
            return [
                p.to_dict()
                for p in self._peers.values()
                if p.mac != exclude_mac
            ]
