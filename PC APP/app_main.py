#!/usr/bin/env python3
"""
app_main.py — MiniHead Control desktop entry point.

Mac:     pywebview (WKWebView) — native window, no browser needed
Windows: Edge/Chrome --app mode — no address bar, looks native

Run with:  python app_main.py
Build with: pyinstaller minihead.spec --clean --noconfirm
"""

import os
import socket
import sys
import threading
import time
import traceback
from pathlib import Path

URL = "http://127.0.0.1:8080"


def _log_dir() -> Path:
    if sys.platform == "darwin":
        p = Path.home() / "Library" / "Application Support" / "MiniHead"
    elif sys.platform == "win32":
        p = Path(os.environ.get("APPDATA", str(Path.home()))) / "MiniHead"
    else:
        p = Path.home() / ".minihead"
    p.mkdir(parents=True, exist_ok=True)
    return p


def _setup_log_file():
    log_path = _log_dir() / "app.log"
    log_file = open(log_path, "w", buffering=1, encoding="utf-8")
    sys.stdout = log_file
    sys.stderr = log_file
    print(f"[MiniHead] Log started — {log_path}")
    return log_path


def _wait_for_flask(retries=50, delay=0.1):
    for _ in range(retries):
        try:
            socket.create_connection(("127.0.0.1", 8080), timeout=0.1).close()
            return True
        except OSError:
            time.sleep(delay)
    return False


def _find_edge_or_chrome():
    """Windows only: find Edge/Chrome for --app mode."""
    candidates = [
        os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft\Edge\Application\msedge.exe"),
        os.path.expandvars(r"%ProgramFiles%\Microsoft\Edge\Application\msedge.exe"),
        os.path.expandvars(r"%LocalAppData%\Microsoft\Edge\Application\msedge.exe"),
        os.path.expandvars(r"%ProgramFiles%\Google\Chrome\Application\chrome.exe"),
        os.path.expandvars(r"%LocalAppData%\Google\Chrome\Application\chrome.exe"),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    return None


def _open_mac():
    """Open a native WKWebView window via pywebview (no browser needed)."""
    import webview
    webview.create_window("MiniHead Control", URL, width=1280, height=900, resizable=True)
    webview.start()


def _open_windows():
    """Open Edge/Chrome in --app mode. Falls back to default browser."""
    import subprocess
    import webbrowser
    browser = _find_edge_or_chrome()
    if browser:
        print(f"[MiniHead] Opening {browser}")
        subprocess.Popen([browser, f"--app={URL}", "--new-window"])
    else:
        webbrowser.open(URL)
    # Keep server alive — Edge may return immediately if already running
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass


def main():
    log_path = _setup_log_file()

    try:
        import logging
        import pc_leader

        with pc_leader._log_flags_lock:
            http_on = pc_leader._log_flags.get("http", False)
        logging.getLogger("werkzeug").setLevel(
            logging.DEBUG if http_on else logging.WARNING
        )

        pc_leader.load_cues()
        pc_leader.load_data()

        threading.Thread(target=pc_leader.beacon_sender,    daemon=True).start()
        threading.Thread(target=pc_leader.beacon_receiver,  daemon=True).start()
        threading.Thread(target=pc_leader.sequencer_runner, daemon=True).start()
        threading.Thread(target=pc_leader.artnet_sniffer,   daemon=True).start()

        flask_thread = threading.Thread(
            target=lambda: pc_leader.app.run(
                host="127.0.0.1", port=8080, debug=False, use_reloader=False
            ),
            daemon=True,
        )
        flask_thread.start()

        if not _wait_for_flask():
            print("[MiniHead] ERROR: Flask did not start — check port 8080 is free")
            sys.exit(1)

        print(f"[MiniHead] Flask running at {URL}")

        if sys.platform == "darwin":
            _open_mac()   # blocks until window is closed → clean exit
        else:
            _open_windows()

    except Exception:
        traceback.print_exc()
        print(f"\n[MiniHead] Crashed — see log: {log_path}")
        time.sleep(10)
        sys.exit(1)


if __name__ == "__main__":
    main()
