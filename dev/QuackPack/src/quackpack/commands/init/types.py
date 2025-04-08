from dataclasses import dataclass
from enum import Enum, auto
from pathlib import Path

from quackpack.util.global_context import GlobalContext


class NewVenvType(Enum):
    Binary = auto()
    PlainVenv = auto()
    Full = auto()


@dataclass(frozen=True, kw_only=True)
class InitOptions:
    destination: Path
    name: str
    type: NewVenvType
    ctx: GlobalContext
