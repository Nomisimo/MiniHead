#!/usr/bin/env python3
"""
app_main.py — MiniHead Control desktop entry point.
Opens the app in Chrome/Edge --app mode (no address bar, looks native).
Zero third-party GUI dependencies — stdlib only.
Run with:  python app_main.py
Build with: pyinstaller minihead.spec --clean --noconfirm
"""

import os
import subprocess
import socket
import sys
import threading
import time
import traceback
import webbrowser
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
    """Redirect stdout + stderr to a log file so errors survive console=False."""
    log_path = _log_dir() / "app.log"
    log_file = open(log_path, "w", buffering=1, encoding="utf-8")
    sys.stdout = log_file
    sys.stderr = log_file
    print(f"[MiniHead] Log started — {log_path}")
    return log_path


def _find_browser():
    if sys.platform == "win32":
        candidates = [
            os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft\Edge\Application\msedge.exe"),
            os.path.expandvars(r"%ProgramFiles%\Microsoft\Edge\Application\msedge.exe"),
            os.path.expandvars(r"%LocalAppData%\Microsoft\Edge\Application\msedge.exe"),
            os.path.expandvars(r"%ProgramFiles%\Google\Chrome\Application\chrome.exe"),
            os.path.expandvars(r"%LocalAppData%\Google\Chrome\Application\chrome.exe"),
        ]
    elif sys.platform == "darwin":
        candidates = [
            "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
            "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
            "/Applications/Chromium.app/Contents/MacOS/Chromium",
        ]
    else:
        candidates = []

    for path in candidates:
        if os.path.isfile(path):
            return path
    return None


def _wait_for_flask(retries=50, delay=0.1):
    for _ in range(retries):
        try:
            socket.create_connection(("127.0.0.1", 8080), timeout=0.1).close()
            return True
        except OSError:
            time.sleep(delay)
    return False


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

        browser = _find_browser()
        if browser:
            print(f"[MiniHead] Opening {browser}")
            subprocess.Popen([browser, f"--app={URL}", "--new-window"])
        else:
            webbrowser.open(URL)

        # Keep the server alive regardless of what the browser process does.
        # (Edge may reuse an existing process and return immediately.)
        print(f"[MiniHead] Server running — close this process to quit")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass

    except Exception:
        traceback.print_exc()
        print(f"\n[MiniHead] Crashed — see log: {log_path}")
        time.sleep(10)  # keep window open so user can read the error
        sys.exit(1)


if __name__ == "__main__":
    main()
