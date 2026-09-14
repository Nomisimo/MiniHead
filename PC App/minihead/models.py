"""Immutable value types for the domain model."""
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Peer:
    mac: str
    ip: str
    fix_id: int
    name: str
    role: str
    mode: str
    last_seen: float

    def to_dict(self) -> dict:
        return {
            "mac":      self.mac,
            "ip":       self.ip,
            "fixID":    self.fix_id,
            "name":     self.name,
            "role":     self.role,
            "mode":     self.mode,
            "lastSeen": self.last_seen,
            "online":   True,
        }


@dataclass
class Fixture:
    id: int
    name: str
    mac: Optional[str] = None

    def to_dict(self, online: bool = False) -> dict:
        return {"id": self.id, "name": self.name, "mac": self.mac, "online": online}


@dataclass
class Cue:
    id: int
    name: str
    r: int
    g: int
    b: int
    w: int
    pan: int
    tilt: int
    fix_targets: list[int] = field(default_factory=lambda: [0])

    def to_dict(self) -> dict:
        return {
            "id":         self.id,
            "name":       self.name,
            "r":          self.r,
            "g":          self.g,
            "b":          self.b,
            "w":          self.w,
            "pan":        self.pan,
            "tilt":       self.tilt,
            "fixTargets": self.fix_targets,
        }

    def dmx_command(self) -> str:
        return f"R:{self.r},G:{self.g},B:{self.b},W:{self.w},PAN:{self.pan},TILT:{self.tilt}"


@dataclass
class Patch:
    fix_id: int
    universe: int
    start_addr: int

    def to_dict(self) -> dict:
        return {"fixID": self.fix_id, "universe": self.universe, "startAddr": self.start_addr}
