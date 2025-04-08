from __future__ import annotations

from typing import Self

from pydantic import model_serializer, model_validator

from quackpack.util.pydantic_helpers import PydanticMutableList
from quackpack.util.version import Version


class VersionListValueError(ValueError):
    index: int

    def __init__(self, index: int, message: str):
        super().__init__(message)
        self.index = index


class VersionList(PydanticMutableList[Version]):
    @classmethod
    def from_raw_data(cls, data: str | list[str | Version]) -> VersionList:
        return VersionList(VersionList._parse(data))

    @classmethod
    def _parse(cls, value: str | list[str | Version]) -> list[Version]:
        if isinstance(value, str):
            return [Version.create_from_string(x.strip()) for x in value.split("or")]
        for idx, version in enumerate(value):
            if isinstance(version, Version):
                continue
            if not isinstance(version, str):  # pyright: ignore reportUnnecessaryIsInstance
                raise VersionListValueError(idx, "value should be a string")
            try:
                value[idx] = Version.create_from_string(version)
            except ValueError as exc:
                raise VersionListValueError(idx, str(exc)) from None
        return value  # pyright: ignore [reportReturnType], we just converted str to Version

    @model_serializer
    def _serialize(self) -> str:
        return " or ".join(str(x) for x in self)

    @model_validator(mode="after")
    def _check_nonempty_versions(self) -> Self:
        if not self:
            raise ValueError("Empty dependency list")
        return self
