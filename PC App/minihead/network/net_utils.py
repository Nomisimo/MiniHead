"""Shared network helpers — interface-aware IP detection."""
import socket
import subprocess
import sys


def get_local_ip_for(target: str) -> str:
    """Return the local IP the OS would use to reach target.

    On macOS with multiple interfaces (WiFi + Thunderbolt) the plain
    UDP-connect trick can return the wrong interface.  This function
    asks the kernel's routing table first via `route -n get`, then
    falls back to UDP-connect probes so it works on Linux too and
    even when the target is temporarily unreachable.
    """
    is_broadcast = target in ("255.255.255.255", "<broadcast>")

    # macOS: ask the kernel which interface it would actually use
    if sys.platform == "darwin" and not is_broadcast:
        try:
            out = subprocess.check_output(
                ["route", "-n", "get", target],
                stderr=subprocess.DEVNULL, timeout=1).decode()
            iface = next(
                (l.split(":")[-1].strip()
                 for l in out.splitlines() if "interface:" in l),
                None)
            if iface:
                ip = subprocess.check_output(
                    ["ipconfig", "getifaddr", iface],
                    stderr=subprocess.DEVNULL, timeout=1).decode().strip()
                if ip and ip not in ("0.0.0.0", "127.0.0.1"):
                    return ip
        except Exception:
            pass

    # UDP-connect fallback (no packets sent — just a route-table lookup)
    for probe in ([target] if not is_broadcast else []) + ["8.8.8.8", "1.1.1.1"]:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.settimeout(0.3)
            s.connect((probe, 1))
            ip = s.getsockname()[0]
            s.close()
            if ip and ip not in ("0.0.0.0", "127.0.0.1"):
                return ip
        except Exception:
            pass

    # Interface scan fallback: parse ifconfig for any IP on the same /24.
    # Handles hotspot/tethered networks where the route table probe above fails.
    if not is_broadcast:
        dst_prefix = ".".join(target.split(".")[:3]) + "."
        try:
            out = subprocess.check_output(
                ["ifconfig"], stderr=subprocess.DEVNULL, timeout=2).decode()
            for line in out.split("\n"):
                if "inet " not in line:
                    continue
                parts = line.strip().split()
                try:
                    idx = parts.index("inet")
                    candidate = parts[idx + 1]
                    if candidate.startswith(dst_prefix) and candidate not in ("0.0.0.0", "127.0.0.1"):
                        return candidate
                except (ValueError, IndexError):
                    pass
        except Exception:
            pass

    return ""
