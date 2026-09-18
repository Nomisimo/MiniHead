import threading
from typing import Optional
from ..models import Patch


class PatchStore:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._patches: list[Patch] = []

    def get_all(self) -> list[Patch]:
        with self._lock:
            return list(self._patches)

    def get_by_fix_id(self, fid: int) -> Optional[Patch]:
        with self._lock:
            return next((p for p in self._patches if p.fix_id == fid), None)

    def upsert(self, fid: int, universe: int, start_addr: int) -> Patch:
        with self._lock:
            self._patches = [p for p in self._patches if p.fix_id != fid]
            patch = Patch(fix_id=fid, universe=universe, start_addr=start_addr)
            self._patches.append(patch)
            return patch

    def update(self, fid: int, universe: Optional[int] = None,
               start_addr: Optional[int] = None) -> Optional[Patch]:
        with self._lock:
            patch = next((p for p in self._patches if p.fix_id == fid), None)
            if not patch:
                return None
            if universe is not None:
                patch.universe = universe
            if start_addr is not None:
                patch.start_addr = start_addr
            return patch

    def delete(self, fid: int) -> bool:
        with self._lock:
            before = len(self._patches)
            self._patches = [p for p in self._patches if p.fix_id != fid]
            return len(self._patches) < before

    def clear(self) -> None:
        with self._lock:
            self._patches.clear()

    def bulk_set(self, patches: list[Patch]) -> None:
        with self._lock:
            self._patches = list(patches)

    def to_list(self) -> list[dict]:
        with self._lock:
            return [p.to_dict() for p in self._patches]

    def load(self, items: list[dict]) -> None:
        with self._lock:
            self._patches = [
                Patch(
                    fix_id=int(d.get("fixID", 0)),
                    universe=int(d.get("universe", 0)),
                    start_addr=int(d.get("startAddr", 1)),
                )
                for d in items
            ]

    def dump(self) -> list[dict]:
        with self._lock:
            return [p.to_dict() for p in self._patches]
