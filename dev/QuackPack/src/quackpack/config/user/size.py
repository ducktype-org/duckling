from __future__ import annotations

from enum import StrEnum
from typing import override

from pydantic import BaseModel, PositiveInt

__all__ = ["Size"]


class Size(BaseModel):
    """
    Class representing disk size with human interface.
    """

    class Unit(StrEnum):
        GiB = "G"
        MiB = "M"
        KiB = "K"

    size: PositiveInt
    unit: Unit

    @classmethod
    def from_string(cls, size: str) -> Size:
        """
        Try to parse a string as a `Size`.
        ----
        Args:
        - `size`: string to be parsed.
        ----
        Returns:
        - `Size`: human parsed size.
        ----
        Raises:
        - `ValueError`: if `size` couldn't be parsed.
        """
        try:
            unit = Size.Unit(size[-1])
            size_as_int = int(size[:-1])
            data = {"size": size_as_int, "unit": unit}
            return Size.model_validate(data)
        except Exception as e:
            raise ValueError(
                f"'{size}' is not a valid size of format <UINT><SUFFIX = ('G', 'M', 'K')>"
            ) from e

    @override
    def __str__(self) -> str:
        return f"{self.size}{self.unit}"
