from pathlib import Path

from pydantic import BaseModel

from .version import Version


class Metadata(BaseModel):
    author: str
    version: Version
    id: str  # TODO: Change to validated (different) type.
    name: str
    license: str
    description: str | None = None
    is_temporary: bool = False


class DependencyConditions(BaseModel):
    system: str | list[str] | None = None
    arch: str | list[str] | None = None
    project_flags: str | list[str] | None = None  # TODO: Convert type to logic string.


# TODO: What actually we need?
class GitEntry(BaseModel):
    git_url: str
    commit: str | None = None
    tag: str | None = None


class LocalEntry(BaseModel):
    path: Path


class DependencyEntry(BaseModel):
    version: str | GitEntry | LocalEntry  # TODO: Version with operators.
    conditions: DependencyConditions | None = None
    flags: list[str | dict[str, DependencyConditions]] | None = None


class Dependencies(BaseModel):
    dependencies: dict[str, list[DependencyEntry]] | None = None


# TODO: Add rest of missing fields.
class Configuration(BaseModel):
    metadata: Metadata
    dependencies: Dependencies | None = None
