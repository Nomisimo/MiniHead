# MiniHead Control — PC App

Control app for MiniHead moving lights. Discovers ESP32 nodes on the network,
manages fixtures, Art-Net patching, cues, and sequencer.

---

## Installation

### macOS

1. Download `MiniHead-Control-mac.zip` from the [Releases page](../../releases)
2. Unzip and drag **MiniHead Control.app** to your Applications folder
3. **First launch:** right-click the app → **Open** → confirm the security dialog
   *(macOS blocks unsigned apps from unknown developers — this is a one-time step)*
4. When prompted by the macOS firewall, click **Allow** to enable network discovery

Your settings and cues are saved in `~/Library/Application Support/MiniHead/`.

### Windows

1. Download `MiniHead-Control-win.zip` from the [Releases page](../../releases)
2. Unzip the folder to any location (e.g. `C:\Program Files\MiniHead Control\`)
3. Run **MiniHead Control.exe**
4. If Windows SmartScreen appears, click **More info → Run anyway**
5. If Edge WebView2 is missing *(rare on Windows 10)*, a small installer will appear
   automatically — follow the prompts (one-time, ~2 MB download)

Your settings and cues are saved in `%APPDATA%\MiniHead\`.

---

## Running from source

Requires Python 3.10+.

```bash
pip install flask
python pc_leader.py
```

Then open [http://localhost:8080](http://localhost:8080) in your browser.

---

## Network ports used

| Port | Protocol | Purpose                   |
|------|----------|---------------------------|
| 8080 | TCP      | Web UI                    |
| 4210 | UDP      | ESP32 beacon discovery    |
| 4211 | UDP      | Commands to ESP nodes     |
| 6454 | UDP      | Art-Net DMX input         |

Make sure your firewall allows these ports on your local network interface.
