from pathlib import Path
from typing import Any, Literal

import yaml


def serialize(*, destination: str | Path, data: Any, mode: Literal["w", "x"] = "w") -> None:
    """
    Serialize `data` as a yaml file in `destination`.
    """
    with open(destination, mode) as f:
        yaml.safe_dump(data, f, default_flow_style=False, sort_keys=False)
