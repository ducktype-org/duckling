from typing import Any

from quackpack.util.version import Version


def convert_to_version(serialized: Any, key: str) -> Any:
    if not isinstance(serialized, dict):
        return serialized
    if key in serialized and isinstance(serialized[key], str):
        serialized[key] = Version.create_from_string(serialized[key])  # pyright: ignore[reportUnknownArgumentType]
    return serialized  # pyright: ignore[reportUnknownVariableType]


# FIXME: More validators from configuration (either user, or project), require better imports (publiczne pliki Ignacy?)
