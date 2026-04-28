# minihead.spec — PyInstaller build spec for MiniHead Control
# Build: pyinstaller minihead.spec --clean --noconfirm
# Output: dist/MiniHead Control/  (folder with executable)
#         dist/MiniHead Control.app  (Mac only)

import sys
import os
from PyInstaller.utils.hooks import collect_submodules

block_cipher = None

_icon_ico  = "assets/icon.ico"  if os.path.exists("assets/icon.ico")  else None
_icon_icns = "assets/icon.icns" if os.path.exists("assets/icon.icns") else None
_icon = _icon_icns if sys.platform == "darwin" else _icon_ico

a = Analysis(
    ["app_main.py"],
    pathex=[],
    binaries=[],
    datas=[
        ("index.html",  "."),
        ("fonts",       "fonts"),
        ("plugins",     "plugins"),
        ("modules",     "modules"),
    ],
    hiddenimports=[
        *collect_submodules("flask"),
        *collect_submodules("werkzeug"),
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=["tkinter", "matplotlib", "numpy", "pandas", "PIL", "cv2"],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="MiniHead Control",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    icon=_icon,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name="MiniHead Control",
)

# Mac: wrap in .app bundle
if sys.platform == "darwin":
    app = BUNDLE(
        coll,
        name="MiniHead Control.app",
        icon=_icon_icns,
        bundle_identifier="com.minihead.control",
        info_plist={
            "NSHighResolutionCapable": True,
            "CFBundleShortVersionString": "4.2",
            "CFBundleName": "MiniHead Control",
        },
    )
