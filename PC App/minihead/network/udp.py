"""Low-level UDP helpers — fire-and-forget command sending."""
import socket
import logging
from .. import config
from .net_utils import get_local_ip_for

log = logging.getLogger(__name__)


def send_udp_cmd(ip: str, mac: str, cmd: str) -> None:
    _send(ip, f"CMD|{mac}|{cmd}")


def send_udp_raw(ip: str, msg: str) -> None:
    _send(ip, msg)


def broadcast_udp_cmd(peers: list, cmd: str) -> None:
    """Unicast cmd to every peer — avoids router broadcast blocking."""
    for ip, mac in peers:
        send_udp_cmd(ip, mac, cmd)


def _send(ip: str, msg: str) -> None:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        local_ip = get_local_ip_for(ip)
        if local_ip and local_ip != "127.0.0.1":
            try:
                sock.bind((local_ip, 0))
            except Exception:
                pass
        sock.sendto(msg.encode(), (ip, config.CMD_PORT))
        sock.close()
    except Exception as e:
        log.warning("[udp] send to %s failed: %s", ip, e)
