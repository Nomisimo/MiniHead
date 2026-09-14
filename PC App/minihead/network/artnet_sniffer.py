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
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        except AttributeError:
            pass
        sock.bind(("", config.ARTNET_PORT))
        sock.settimeout(1.0)

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
                log.warning("[artnet] %s", e)

            changed = self._state.update_active()
            if changed:
                active = self._state.is_active()
                log.info("[artnet] stream %s", "started" if active else "stopped")
