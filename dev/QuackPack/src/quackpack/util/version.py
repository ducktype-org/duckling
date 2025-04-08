from __future__ import annotations

from dataclasses import dataclass
from typing import override

from pydantic import model_serializer

__all__ = ["Version"]


@dataclass(order=True, frozen=True)
class Version:
    """
    SemVer human interface.
    """

    major: int
    """
    Package major version. Should be non-negative.
    """
    minor: int = 0
    """
    Package minor version. Should be non-negative.
    """
    patch: int = 0
    """
    Package patch version. Should be non-negative.
    """

    def __post_init__(self) -> None:
        if self.major < 0 or self.minor < 0 or self.patch < 0:
            raise ValueError("Negative versions are not supported")
        if self.major == 0 and self.minor == 0:
            raise ValueError("Versions of format 0.0.X are not valid SemVer")

    @classmethod
    def create_from_string(cls, string: str) -> Version:
        """
        Create `Version` from given `input`.
        ----
        Args:
        - `input`: string to be parsed as `Version`.
        ----
        Returns:
        - `Version`: parsed `Version` from `input`.
        ----
        Raises:
        - `ValueError`: if `input` is invalid (too many keys or non-numeric ones).
        """
        split = string.split(".")
        if len(split) > 3:
            raise ValueError("Too many arguments")
        return Version(*(int(x) for x in split))

    def can_be_upgraded_to(self, other: Version) -> bool:
        """
        Check if `self` can be upgraded to `other` without breaking changes (as defined by SemVer).
        ----
        Args:
        - `other`: other `Version` to compare against.
        ----
        Returns:
        - `bool`: `True` if `self` can be upgraded to `other` without breaking changes.
        """
        if self.major != other.major:
            return False
        if self.major == 0:
            return self.minor == other.minor and self.patch <= other.patch
        return self <= other

    def bump_patch(self) -> Version:
        """
        Bump patch version.
        ----
        Returns:
        - `Version`: new `Version` with increased patch version.
        """
        return Version(self.major, self.minor, self.patch + 1)

    def bump_minor(self) -> Version:
        """
        Bump minor version.
        New patch will be set to 0.
        ----
        Returns:
        - `Version`: new `Version` with increased minor version.
        """
        return Version(self.major, self.minor + 1, 0)

    def bump_major(self) -> Version:
        """
        Bump major version.
        New patch and minor will be set to 0.
        ----
        Returns:
        - `Version`: new `Version` with increased major version.
        """
        return Version(self.major + 1, 0, 0)

    @override
    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}"

    @model_serializer
    def serialize(self) -> str:
        return str(self)

    @classmethod
    def default(cls) -> Version:
        return Version.create_from_string("0.1")
