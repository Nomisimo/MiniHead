import threading
import time
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from ..state.patches import PatchStore

from .. import config


class ArtNetState:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._universes: dict[int, bytearray] = {}
        self._last_packet: float = 0.0
        self._active: bool = False
        # Cached last-decoded values for the status endpoint
        self._last_values: dict = {"r": 0, "g": 0, "b": 0, "w": 0, "pan": 0, "tilt": 0}

    def receive(self, universe: int, dmx_data: bytes) -> None:
        with self._lock:
            self._universes[universe] = bytearray(dmx_data)
            self._last_packet = time.time()
            self._active = True

    def update_active(self) -> bool:
        with self._lock:
            was = self._active
            self._active = (time.time() - self._last_packet) < config.ARTNET_TIMEOUT_S
            changed = was != self._active
            return changed

    def is_active(self) -> bool:
        with self._lock:
            return self._active

    def decode_first_patch(self, patches: list) -> None:
        """Decode DMX values from the first matching patch (display only)."""
        with self._lock:
            for patch in patches:
                uni  = patch.universe
                addr = patch.start_addr - 1  # 0-indexed
                dmx  = self._universes.get(uni, b"")
                if len(dmx) < addr + 7:
                    continue
                master = dmx[addr]
                self._last_values = {
                    "r":    dmx[addr + 1] * master // 255,
                    "g":    dmx[addr + 2] * master // 255,
                    "b":    dmx[addr + 3] * master // 255,
                    "w":    dmx[addr + 4] * master // 255,
                    "pan":  round(dmx[addr + 5] / 255 * 270),
                    "tilt": round(dmx[addr + 6] / 255 * 270),
                }
                break

    def status(self, patch_count: int) -> dict:
        with self._lock:
            return {
                "active":     self._active,
                "patchCount": patch_count,
                **self._last_values,
            }
