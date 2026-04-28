#!/usr/bin/env python3
"""
app_main.py — MiniHead Control desktop entry point.
Opens the app in Chrome/Edge app-mode window (no .NET / WebView2 runtime needed).
Run with:  python app_main.py
Build with: pyinstaller minihead.spec --clean --noconfirm
"""

import threading
import logging
import sys
from flaskwebgui import FlaskUI


def main():
    import pc_leader

    # Mirror the log-level setup from pc_leader's __main__ block
    with pc_leader._log_flags_lock:
        http_on = pc_leader._log_flags.get("http", False)
    logging.getLogger("werkzeug").setLevel(
        logging.DEBUG if http_on else logging.WARNING
    )

    # Load persisted data
    pc_leader.load_cues()
    pc_leader.load_data()

    # Start background services
    threading.Thread(target=pc_leader.beacon_sender,    daemon=True).start()
    threading.Thread(target=pc_leader.beacon_receiver,  daemon=True).start()
    threading.Thread(target=pc_leader.sequencer_runner, daemon=True).start()
    threading.Thread(target=pc_leader.artnet_sniffer,   daemon=True).start()

    print(f"[PC Leader] {pc_leader.APP_VERSION}")
    print(f"[PC Leader] MAC:  {pc_leader.OWN_MAC}")
    print(f"[PC Leader] IP:   {pc_leader.OWN_IP}")

    ui = FlaskUI(
        app=pc_leader.app,
        server="flask",
        host="127.0.0.1",
        port=8080,
        width=1280,
        height=900,
        fullscreen=False,
    )
    ui.run()


if __name__ == "__main__":
    main()
