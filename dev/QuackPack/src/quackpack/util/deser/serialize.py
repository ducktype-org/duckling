from pathlib import Path
from typing import Any, Literal

import yaml


def serialize(*, destination: str | Path, data: Any, mode: Literal["w", "x"] = "w") -> None:
    with open(destination, mode) as f:
        yaml.safe_dump(data, f, default_flow_style=False, sort_keys=False)
