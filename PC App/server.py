"""
MiniHead PC Leader v3
Entry point — run with:  python server.py
"""
import logging
from minihead.app import create_app
from minihead import config

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s %(message)s",
    datefmt="%H:%M:%S",
)

if __name__ == "__main__":
    print(f"MiniHead PC Leader v{config.APP_VERSION}  →  http://0.0.0.0:{config.HTTP_PORT}")
    app = create_app()
    try:
        from waitress import serve
        print("[server] waitress")
        serve(app, host="0.0.0.0", port=config.HTTP_PORT, threads=8)
    except ImportError:
        print("[server] Flask dev server (install waitress for production)")
        app.run(host="0.0.0.0", port=config.HTTP_PORT, debug=False, threaded=True)
