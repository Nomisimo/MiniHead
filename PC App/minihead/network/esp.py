"""HTTP client for talking to ESP32 devices."""
import http.client
import json
import logging
from typing import Optional
from .net_utils import get_local_ip_for

log = logging.getLogger(__name__)
_TIMEOUT = 3


def _request(ip: str, method: str, path: str, body: dict | None = None) -> Optional[bytes]:
    """Open an HTTP connection bound to the correct local interface."""
    local_ip = get_local_ip_for(ip)
    src = (local_ip, 0) if local_ip and local_ip != "127.0.0.1" else None
    conn = http.client.HTTPConnection(ip, timeout=_TIMEOUT, source_address=src)
    try:
        headers: dict = {}
        data: bytes | None = None
        if body is not None:
            data = json.dumps(body).encode()
            headers["Content-Type"] = "application/json"
        conn.request(method, path, body=data, headers=headers)
        resp = conn.getresponse()
        return resp.read()
    finally:
        conn.close()


def http_get(ip: str, path: str) -> Optional[dict | list]:
    try:
        raw = _request(ip, "GET", path)
        return json.loads(raw) if raw else None
    except Exception as e:
        log.warning("[esp] GET %s%s: %s", ip, path, e)
        return None


def http_post(ip: str, path: str, body: dict) -> bool:
    try:
        _request(ip, "POST", path, body)
        return True
    except Exception as e:
        log.warning("[esp] POST %s%s: %s", ip, path, e)
        return False


def http_delete(ip: str, path: str) -> bool:
    try:
        _request(ip, "DELETE", path)
        return True
    except Exception as e:
        log.warning("[esp] DELETE %s%s: %s", ip, path, e)
        return False


def push_patch(ip: str, mac: str, fix_id: int, universe: int, start_addr: int) -> bool:
    log.info("[patch] push Fix#%d U%d A%d → %s (%s)", fix_id, universe, start_addr, mac, ip)
    return http_post(ip, "/api/artnet/patch", {
        "fixID": fix_id, "universe": universe, "startAddr": start_addr,
    })


def delete_patch(ip: str, mac: str, fix_id: int) -> None:
    log.info("[patch] delete Fix#%d from %s (%s)", fix_id, mac, ip)
    http_delete(ip, f"/api/artnet/patch/{fix_id}")


def push_log_flags(ip: str, mac: str, flags: dict) -> None:
    log.debug("[logflags] push to %s (%s)", mac, ip)
    http_post(ip, "/api/logconfig", flags)


def poll_patches(ip: str, fix_id: int, name: str) -> list[dict]:
    """HTTP-poll one ESP for its Art-Net patch list."""
    data = http_get(ip, "/api/artnet/patch")
    if not isinstance(data, list):
        return []
    result = []
    for p in data:
        result.append({
            "ip":        ip,
            "name":      name,
            "fixID":     p.get("fixID", fix_id),
            "universe":  p.get("universe", 0),
            "startAddr": p.get("startAddr", 1),
        })
    log.info("[artpoll] HTTP %s: %d patch(es)", ip, len(result))
    return result
