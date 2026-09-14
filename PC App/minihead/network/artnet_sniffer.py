"""ArtNetSniffer — listens on UDP 6454 and feeds ArtNetState."""
import socket
import struct
import threading
import logging
from ..state.artnet import ArtNetState
from ..state.patches import PatchStore
from .. import config

log = logging.getLogger(__name__)


class ArtNetSniffer:
    def __init__(self, artnet_state: ArtNetState, patches: PatchStore) -> None:
        self._state   = artnet_state
        self._patches = patches

    def start(self) -> None:
        t = threading.Thread(target=self._run, daemon=True, name="artnet_sniffer")
        t.start()
        log.info("[init] thread: artnet_sniffer")

    def _run(self) -> None:
        import time
        sock = self._bind_with_retry()
        if sock is None:
            return  # gave up after retries; error already logged

        while True:
            try:
                data, _ = sock.recvfrom(600)
                if len(data) < 18 or data[:8] != b"Art-Net\0":
                    continue
                opcode = struct.unpack_from("<H", data, 8)[0]
                if opcode != 0x5000:  # ArtDmx
                    continue
                universe = struct.unpack_from("<H", data, 14)[0]
                length   = struct.unpack_from(">H", data, 16)[0]
                dmx_data = data[18: 18 + length]
                self._state.receive(universe, dmx_data)
                self._state.decode_first_patch(self._patches.get_all())
            except socket.timeout:
                pass
            except Exception as e:
                log.warning("[artnet] recv error: %s", e)

            changed = self._state.update_active()
            if changed:
                active = self._state.is_active()
                log.info("[artnet] stream %s", "started" if active else "stopped")

    def _bind_with_retry(self, retries: int = 5, delay: float = 5.0):
        import time
        for attempt in range(1, retries + 1):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                try:
                    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
                except AttributeError:
                    pass
                sock.bind(("", config.ARTNET_PORT))
                sock.settimeout(1.0)
                log.info("[artnet] listening on UDP %d", config.ARTNET_PORT)
                return sock
            except OSError as e:
                log.error(
                    "[artnet] cannot bind port %d (attempt %d/%d): %s — "
                    "is another Art-Net app running?",
                    config.ARTNET_PORT, attempt, retries, e,
                )
                time.sleep(delay)
        log.error("[artnet] gave up after %d attempts — Art-Net monitoring disabled", retries)
        return None
