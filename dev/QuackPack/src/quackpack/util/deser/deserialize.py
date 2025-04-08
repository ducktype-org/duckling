from pathlib import Path

from pydantic import BaseModel

from .yaml.strict_parsing import load_and_validate


def deserialize[T: BaseModel](source: Path, type_: type[T]) -> T:
    return load_and_validate(source, type_)
