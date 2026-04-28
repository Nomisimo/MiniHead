#!/usr/bin/env python3
"""
Convert assets/icon.png to icon.ico (Windows) and icon.icns (Mac).
Run before PyInstaller: python assets/convert_icon.py
Requires: pip install Pillow
"""

import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
SRC  = HERE / "icon.png"

SIZES = [16, 32, 48, 64, 128, 256, 512]


def make_ico(img):
    out = HERE / "icon.ico"
    ico_sizes = [(s, s) for s in [16, 32, 48, 64, 128, 256]]
    img.save(out, format="ICO", sizes=ico_sizes)
    print(f"  created {out}")


def make_icns(img):
    """Build .icns via macOS iconutil (only works on macOS)."""
    iconset = HERE / "icon.iconset"
    iconset.mkdir(exist_ok=True)

    icns_sizes = {
        "icon_16x16.png":       16,
        "icon_16x16@2x.png":    32,
        "icon_32x32.png":       32,
        "icon_32x32@2x.png":    64,
        "icon_128x128.png":     128,
        "icon_128x128@2x.png":  256,
        "icon_256x256.png":     256,
        "icon_256x256@2x.png":  512,
        "icon_512x512.png":     512,
    }

    for fname, size in icns_sizes.items():
        resized = img.resize((size, size))
        resized.save(iconset / fname, format="PNG")

    out = HERE / "icon.icns"
    result = subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(out)])
    shutil.rmtree(iconset)

    if result.returncode == 0:
        print(f"  created {out}")
    else:
        print("  iconutil failed — skipping .icns", file=sys.stderr)


if __name__ == "__main__":
    if not SRC.exists():
        print(f"ERROR: {SRC} not found — place your icon as assets/icon.png")
        sys.exit(1)

    from PIL import Image
    img = Image.open(SRC).convert("RGBA")

    print("Converting icon...")
    make_ico(img)
    if sys.platform == "darwin":
        make_icns(img)
    else:
        print("  skipping .icns (only built on macOS)")
    print("Done.")
