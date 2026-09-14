"""Central constants — the only place to change ports, timeouts, etc."""
import os

# ── Ports ──────────────────────────────────────────────────────────────────
BEACON_PORT   = 4210
CMD_PORT      = 4211
ARTNET_PORT   = 6454
HTTP_PORT     = 8080

# ── Timing ────────────────────────────────────────────────────────────────
PEER_TIMEOUT    = 90.0   # seconds without a beacon before a peer is expired
KEEPALIVE_INT   = 10.0   # HTTP keepalive poll interval
BEACON_INT      = 1.0    # how often we broadcast our own beacon
EXPIRY_LOOP_INT = 30.0   # background expiry sweep interval

# ── Identity ──────────────────────────────────────────────────────────────
APP_VERSION = "3.0"
OWN_MAC     = "00:00:00:00:00:PC"

# ── Paths ─────────────────────────────────────────────────────────────────
BASE_DIR   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_DIR   = os.path.join(BASE_DIR, "data")
STATIC_DIR = os.path.join(BASE_DIR, "static")

DATA_FILE  = os.path.join(DATA_DIR, "data.json")
CUES_FILE  = os.path.join(DATA_DIR, "cues.json")
FLAGS_FILE = os.path.join(DATA_DIR, "flags.json")

# ── Default log flags ─────────────────────────────────────────────────────
DEFAULT_LOG_FLAGS: dict[str, bool] = {
    "artnetFrames":     False,
    "artnetEvents":     True,
    "discoveryBeacons": False,
    "discoveryEvents":  True,
    "udpVerbose":       True,
}

# ── ArtNet ────────────────────────────────────────────────────────────────
DMX_CHANNELS_PER_FIXTURE = 7
ARTNET_TIMEOUT_S = 3.0   # seconds without a packet before stream is "inactive"
