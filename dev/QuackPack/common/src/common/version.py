from __future__ import annotations

from dataclasses import dataclass
from typing import Annotated, Any

from pydantic import BeforeValidator


# TODO: Refactor this.
@dataclass(order=True, frozen=True)
class Version:
    """
    SemVer human interface.
    """

    major: int
    """
    Package major version. Should be nonnegative.
    """
    minor: int = 0
    """
    Package minor version. Should be nonnegative.
    """
    patch: int = 0
    """
    Package patch version. Should be nonnegative.
    """

    def __post_init__(self):
        if self.major < 0 or self.minor < 0 or self.patch < 0:
            raise ValueError("Negative versions are not supported")
        if self.major == 0 and self.minor == 0:
            raise ValueError("Versions of format 0.0.X are not valid SemVer")

    @staticmethod
    def create_from_string(input: str) -> Version:
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
        split = input.split(".")
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

    def __str__(self) -> str:
        if self.minor == 0 and self.patch == 0:
            return f"{self.major}"
        if self.patch == 0:
            return f"{self.major}.{self.minor}"
        return f"{self.major}.{self.minor}.{self.patch}"


def version_list_parser(value: Any) -> Any:
    # If we get a list or a Version then we do not need to do anything
    if isinstance(value, list):
        if len(value) == 0:  # type: ignore[attr-defined]
            raise ValueError("Empty list")
        result: list[Version] = []
        for x in value:  # type: ignore[attr-defined]
            if not isinstance(x, str):
                raise ValueError("All list elements should be strings")
            result.append(Version.create_from_string(x))
        return result
    if isinstance(value, str):
        return [Version.create_from_string(x.strip()) for x in value.split("or")]
    raise ValueError("Input should be a list or string")


type VersionList = Annotated[list[Version], BeforeValidator(version_list_parser)]
