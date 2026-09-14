"""HTTP client for talking to ESP32 devices."""
import json
import logging
import urllib.request
from typing import Optional

log = logging.getLogger(__name__)
_TIMEOUT = 3


def http_get(ip: str, path: str) -> Optional[dict | list]:
    try:
        resp = urllib.request.urlopen(f"http://{ip}{path}", timeout=_TIMEOUT)
        data = json.loads(resp.read())
        resp.close()
        return data
    except Exception as e:
        log.warning("[esp] GET %s%s: %s", ip, path, e)
        return None


def http_post(ip: str, path: str, body: dict) -> bool:
    try:
        req = urllib.request.Request(
            f"http://{ip}{path}",
            data=json.dumps(body).encode(),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        urllib.request.urlopen(req, timeout=_TIMEOUT).read()
        return True
    except Exception as e:
        log.warning("[esp] POST %s%s: %s", ip, path, e)
        return False


def http_delete(ip: str, path: str) -> bool:
    try:
        req = urllib.request.Request(f"http://{ip}{path}", method="DELETE")
        urllib.request.urlopen(req, timeout=_TIMEOUT).read()
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
