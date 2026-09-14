import threading
from typing import Optional
from ..models import Cue


class CueStore:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._cues: list[Cue] = []
        self._next_id: int = 1

    def get_all(self) -> list[Cue]:
        with self._lock:
            return list(self._cues)

    def get(self, cid: int) -> Optional[Cue]:
        with self._lock:
            return next((c for c in self._cues if c.id == cid), None)

    def create(self, name: str, r: int, g: int, b: int, w: int,
               pan: int, tilt: int, fix_targets: list[int]) -> Cue:
        with self._lock:
            cue = Cue(
                id=self._next_id,
                name=name[:64],
                r=max(0, min(255, r)),
                g=max(0, min(255, g)),
                b=max(0, min(255, b)),
                w=max(0, min(255, w)),
                pan=max(0, min(270, pan)),
                tilt=max(0, min(270, tilt)),
                fix_targets=fix_targets or [0],
            )
            self._next_id += 1
            self._cues.append(cue)
            return cue

    def delete(self, cid: int) -> bool:
        with self._lock:
            idx = next((i for i, c in enumerate(self._cues) if c.id == cid), None)
            if idx is None:
                return False
            self._cues.pop(idx)
            return True

    def update_targets(self, cid: int, fix_targets: list[int]) -> bool:
        with self._lock:
            cue = next((c for c in self._cues if c.id == cid), None)
            if not cue:
                return False
            cue.fix_targets = fix_targets or [0]
            return True

    def reorder(self, order: list[int]) -> bool:
        with self._lock:
            by_id = {c.id: c for c in self._cues}
            reordered = [by_id[oid] for oid in order if oid in by_id]
            if len(reordered) != len(self._cues):
                return False
            self._cues[:] = reordered
            return True

    def to_list(self) -> list[dict]:
        with self._lock:
            return [c.to_dict() for c in self._cues]

    def load(self, items: list[dict]) -> None:
        with self._lock:
            self._cues = [
                Cue(
                    id=int(d["id"]),
                    name=str(d.get("name", "Cue"))[:64],
                    r=max(0, min(255, int(d.get("r", 0)))),
                    g=max(0, min(255, int(d.get("g", 0)))),
                    b=max(0, min(255, int(d.get("b", 0)))),
                    w=max(0, min(255, int(d.get("w", 0)))),
                    pan=max(0, min(270, int(d.get("pan", 90)))),
                    tilt=max(0, min(270, int(d.get("tilt", 45)))),
                    fix_targets=list(d.get("fixTargets", [0])) or [0],
                )
                for d in items
            ]
            if self._cues:
                self._next_id = max(c.id for c in self._cues) + 1

    def dump(self) -> list[dict]:
        with self._lock:
            return [c.to_dict() for c in self._cues]
