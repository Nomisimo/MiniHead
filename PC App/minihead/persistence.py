"""JSON file persistence — load/save for fixtures, patches, cues, log flags."""
import json
import os
import logging
from . import config

log = logging.getLogger(__name__)


def _read(path: str) -> dict | list | None:
    if not os.path.exists(path):
        return None
    try:
        with open(path) as f:
            return json.load(f)
    except Exception as e:
        log.error("[persist] read %s: %s", path, e)
        return None


def _write(path: str, data) -> None:
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(data, f, indent=2)
    except Exception as e:
        log.error("[persist] write %s: %s", path, e)


def load_all(fixtures, patches, cues, log_flags: dict) -> None:
    d = _read(config.DATA_FILE)
    if isinstance(d, dict):
        fixtures.load(d.get("fixtures", {}))
        patches.load(d.get("patches", []))

    c = _read(config.CUES_FILE)
    if isinstance(c, dict):
        cues.load(c.get("cues", []))

    fl = _read(config.FLAGS_FILE)
    if isinstance(fl, dict):
        for k in log_flags:
            if k in fl:
                log_flags[k] = bool(fl[k])


def save_data(fixtures, patches) -> None:
    _write(config.DATA_FILE, {
        "fixtures": fixtures.dump(),
        "patches":  patches.dump(),
    })


def save_cues(cues) -> None:
    _write(config.CUES_FILE, {"cues": cues.dump()})


def save_flags(log_flags: dict) -> None:
    _write(config.FLAGS_FILE, dict(log_flags))
