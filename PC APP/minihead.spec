# minihead.spec — PyInstaller build spec for MiniHead Control
# Build: pyinstaller minihead.spec --clean --noconfirm
# Output: dist/MiniHead Control/  (folder with executable)
#         dist/MiniHead Control.app  (Mac only)

import sys
from PyInstaller.utils.hooks import collect_submodules, collect_all

block_cipher = None

_fg_datas, _fg_binaries, _fg_hidden = collect_all("flaskwebgui")

a = Analysis(
    ["app_main.py"],
    pathex=[],
    binaries=[*_fg_binaries],
    datas=[
        ("index.html",  "."),
        ("fonts",       "fonts"),
        ("plugins",     "plugins"),
        ("modules",     "modules"),
        *_fg_datas,
    ],
    hiddenimports=[
        *collect_submodules("flask"),
        *collect_submodules("werkzeug"),
        *_fg_hidden,
        "flaskwebgui",
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
    console=False,   # set True temporarily to see startup errors
    icon=None,       # replace with "assets/icon.icns" / "assets/icon.ico" if you have one
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
        icon=None,   # replace with "assets/icon.icns" if you have one
        bundle_identifier="com.minihead.control",
        info_plist={
            "NSHighResolutionCapable": True,
            "CFBundleShortVersionString": "4.2",
            "CFBundleName": "MiniHead Control",
        },
    )
