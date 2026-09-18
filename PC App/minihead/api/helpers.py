"""Shared response helpers for all blueprints."""
from flask import request, jsonify


def ok(**extra) -> tuple:
    return jsonify({"status": "ok", **extra}), 200


def err(message: str, code: int = 400) -> tuple:
    return jsonify({"error": message}), code


def require_json() -> dict | None:
    """Return parsed JSON body or None if missing/invalid."""
    data = request.get_json(force=True, silent=True)
    return data if isinstance(data, dict) else None
