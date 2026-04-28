#!/usr/bin/env python3
"""
app_main.py — MiniHead Control desktop entry point.
Starts the Flask server in a background thread, then opens a native OS window.
Run with:  python app_main.py
Build with: pyinstaller minihead.spec --clean --noconfirm
"""

import threading
import time
import socket
import sys


def _wait_for_flask(host: str, port: int, retries: int = 50, delay: float = 0.1) -> bool:
    for _ in range(retries):
        try:
            socket.create_connection((host, port), timeout=0.1).close()
            return True
        except OSError:
            time.sleep(delay)
    return False


def main():
    import pc_leader  # noqa: registers all Flask routes as a side effect

    flask_thread = threading.Thread(
        target=lambda: pc_leader.app.run(
            host="127.0.0.1", port=8080, debug=False, use_reloader=False
        ),
        daemon=True,
    )
    flask_thread.start()

    if not _wait_for_flask("127.0.0.1", 8080):
        print("[MiniHead] ERROR: Flask did not start in time", file=sys.stderr)
        sys.exit(1)

    import webview
    webview.create_window(
        "MiniHead Control",
        "http://127.0.0.1:8080",
        width=1280,
        height=900,
        resizable=True,
    )
    # Force Edge WebView2 on Windows — avoids the pythonnet/winforms .NET error
    gui = "edgechromium" if sys.platform == "win32" else None
    webview.start(gui=gui)


if __name__ == "__main__":
    main()
