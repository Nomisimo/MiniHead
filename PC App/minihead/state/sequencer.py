import threading
import time
from typing import Optional


class SequencerEngine:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._running = False
        self._cue_ids: list[int] = []
        self._interval_ms: int = 2000
        self._loop: bool = True
        self._index: int = 0
        self._last_step: float = 0.0

    def start(self, cue_ids: list[int], interval_ms: int, loop: bool) -> None:
        with self._lock:
            self._cue_ids = list(cue_ids)
            self._interval_ms = max(100, interval_ms)
            self._loop = loop
            self._index = 0
            self._last_step = 0.0   # fire first cue immediately
            self._running = bool(cue_ids)

    def stop(self) -> None:
        with self._lock:
            self._running = False

    def tick(self, now: float) -> Optional[int]:
        """Call frequently. Returns the next cue ID to fire, or None."""
        with self._lock:
            if not self._running or not self._cue_ids:
                return None
            if (now - self._last_step) * 1000 < self._interval_ms:
                return None
            self._last_step = now
            idx = self._index % len(self._cue_ids)
            cue_id = self._cue_ids[idx]
            self._index = idx + 1
            if self._index >= len(self._cue_ids):
                if self._loop:
                    self._index = 0
                else:
                    self._running = False
            return cue_id

    def status(self) -> dict:
        with self._lock:
            return {
                "running":     self._running,
                "cue_ids":     list(self._cue_ids),
                "interval_ms": self._interval_ms,
                "loop":        self._loop,
                "index":       self._index,
            }
