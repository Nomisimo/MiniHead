"""Blueprint: /api/sequencer — start/stop/status."""
from flask import Blueprint, jsonify
from .helpers import ok, err, require_json
from ..state.sequencer import SequencerEngine


def create_sequencer_bp(sequencer: SequencerEngine) -> Blueprint:
    bp = Blueprint("sequencer", __name__, url_prefix="/api/sequencer")

    @bp.post("/start")
    def start():
        data = require_json()
        if data is None:
            return err("JSON body required")
        cue_ids     = [int(x) for x in data.get("cue_ids", [])]
        interval_ms = int(data.get("interval_ms", 2000))
        loop        = bool(data.get("loop", True))
        if not cue_ids:
            return err("cue_ids required")
        sequencer.start(cue_ids, interval_ms, loop)
        return ok()

    @bp.post("/stop")
    def stop():
        sequencer.stop()
        return ok()

    @bp.get("/status")
    def status():
        return jsonify(sequencer.status())

    return bp
