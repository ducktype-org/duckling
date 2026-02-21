from __future__ import annotations

from enum import StrEnum
from typing import Any, cast

from strictyaml import (  # pyright: ignore[reportMissingTypeStubs]
    YAML,
    as_document,  # pyright: ignore[reportUnknownVariableType]
    dirty_load,  # pyright: ignore[reportUnknownVariableType]
)
from strictyaml.ruamel.comments import (
    CommentedMap,
)  # pyright: ignore[reportMissingTypeStubs]

from quackpack.util.types.errors import QuackPackError


# NOTE: Keep in sync with `schemas::ManifestSchema`.
class Section(StrEnum):
    DEPS = "dependencies"
    DEV_DEPS = "dev_dependencies"

    # `x.as_str()` >>>> `str(x)`
    def as_str(self) -> str:
        return str(self)


class EditableManifest:
    def __init__(self, content: YAML):
        self._inner: YAML = content

    @classmethod
    def load(cls, content: str) -> EditableManifest:
        return cls(cast(YAML, dirty_load(content, allow_flow_style=True)))

    @classmethod
    def create_with_data(cls, data: dict[str, Any]) -> EditableManifest:
        return cls(as_document(data))

    def get_table_or_insert_if_absent(self, section: Section) -> dict[str, Any]:
        key = section.as_str()
        inner = cast(CommentedMap, self._inner.as_marked_up())
        if key not in inner:
            inner[key] = {}
        ret: Any = inner[key]  # pyright: ignore[reportUnknownVariableType]
        if not isinstance(ret, dict):
            raise QuackPackError(
                f"editable manifest section `{key}` does not point to a dictionary"
            )
        return ret  # pyright: ignore[reportUnknownVariableType]

    def get_table(self, section: Section) -> dict[str, Any] | None:
        key = section.as_str()
        inner = cast(CommentedMap, self._inner.as_marked_up())
        if key not in inner:
            return None
        ret: Any = inner[key]  # pyright: ignore[reportUnknownVariableType]
        if not isinstance(ret, dict):
            raise QuackPackError(
                f"editable manifest section `{key}` does not point to a dictionary"
            )
        return ret  # pyright: ignore[reportUnknownVariableType]

    def must_get_table(self, section: Section) -> dict[str, Any]:
        ret = self.get_table(section)
        if ret is None:
            raise QuackPackError(f"there is no such section as `{section.as_str()}`")
        return ret

    def insert_into_table_with_override(
        self, section: Section, key: str, data: Any
    ) -> None:
        table = self.get_table_or_insert_if_absent(section)
        table[key] = data

    def insert_into_table_if_absent(
        self, section: Section, key: str, data: Any
    ) -> None:
        table = self.get_table_or_insert_if_absent(section)
        if key in table:
            raise QuackPackError(
                f"cannot override key `{key}` of table `{section.as_str()}`"
            )
        table[key] = data

    def as_str(self) -> str:
        return cast(str, self._inner.as_yaml())
