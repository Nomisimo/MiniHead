import threading
from typing import Optional
from ..models import Fixture


class FixtureStore:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._fixtures: dict[int, Fixture] = {}

    def get(self, fid: int) -> Optional[Fixture]:
        with self._lock:
            return self._fixtures.get(fid)

    def get_all(self) -> list[Fixture]:
        with self._lock:
            return sorted(self._fixtures.values(), key=lambda f: f.id)

    def upsert(self, fid: int, name: str, mac: Optional[str] = None) -> Fixture:
        with self._lock:
            existing = self._fixtures.get(fid)
            f = Fixture(
                id=fid,
                name=name or (existing.name if existing else f"Fix {fid}"),
                mac=mac if mac is not None else (existing.mac if existing else None),
            )
            self._fixtures[fid] = f
            return f

    def update(self, fid: int, **kwargs) -> Optional[Fixture]:
        with self._lock:
            f = self._fixtures.get(fid)
            if not f:
                return None
            if "name" in kwargs:
                f.name = str(kwargs["name"])[:32]
            if "mac" in kwargs:
                f.mac = kwargs["mac"] or None
            return f

    def delete(self, fid: int) -> bool:
        with self._lock:
            return self._fixtures.pop(fid, None) is not None

    def mac_for_fix_id(self, fid: int) -> Optional[str]:
        with self._lock:
            f = self._fixtures.get(fid)
            return f.mac if f else None

    def fix_id_for_mac(self, mac: str) -> Optional[int]:
        with self._lock:
            for f in self._fixtures.values():
                if f.mac == mac:
                    return f.id
            return None

    def to_list(self, online_macs: set[str]) -> list[dict]:
        with self._lock:
            return [
                f.to_dict(online=bool(f.mac and f.mac in online_macs))
                for f in sorted(self._fixtures.values(), key=lambda x: x.id)
            ]

    def load(self, data: dict[str, dict]) -> None:
        with self._lock:
            self._fixtures = {
                int(k): Fixture(id=int(k), name=v.get("name", ""), mac=v.get("mac"))
                for k, v in data.items()
            }

    def dump(self) -> dict:
        with self._lock:
            return {
                str(f.id): {"name": f.name, "mac": f.mac}
                for f in self._fixtures.values()
            }
