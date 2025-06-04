from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import override

from quackpack.config.project import DependencyConditions, DependencyEntry, GitEntry, LocalEntry
from quackpack.solver.universal_names import UniversalName
from quackpack.util.pkgid import Identifier
from quackpack.util.version import Version


class NoFlags(Enum):
    NO_FLAGS = 0


def check_conditions(origin_flag: str | NoFlags, conditions: DependencyConditions | None) -> bool:
    if conditions is None:
        return True

    # TODO: Check arch and system
    if conditions.project_flags is None:
        return True
    else:
        return origin_flag is not NoFlags.NO_FLAGS and origin_flag in (
            (conditions.project_flags.root,)
            if isinstance(conditions.project_flags, Identifier)
            else conditions.project_flags
        )


def check_conditions_any(origin_flags: list[str | NoFlags], conditions: DependencyConditions | None) -> bool:
    return any(check_conditions(flag, conditions) for flag in origin_flags)


def get_flags(origin_flag: NoFlags | str, dep_entry: DependencyEntry) -> list[NoFlags | str]:
    flags_to_check: list[NoFlags | str] = [NoFlags.NO_FLAGS]
    for flag_entry in dep_entry.flags:
        if isinstance(flag_entry, Identifier):
            flags_to_check.append(flag_entry.root)
        else:
            for flag, condition in flag_entry.items():
                # TODO: Is this right, or should it be check_conditions(flag, condition)?
                if check_conditions(origin_flag, condition):
                    flags_to_check.append(flag.root)
    return flags_to_check


# A class to identify single package version.
class SolverPackage:
    name: UniversalName
    version: Version | GitEntry | LocalEntry

    def __init__(self, name: UniversalName, version: Version | GitEntry | LocalEntry):
        self.name = name
        self.version = version

    @override
    def __eq__(self, other: object) -> bool:
        if not isinstance(other, SolverPackage):
            return False
        return self.name == other.name and self.version == other.version

    @override
    def __hash__(self) -> int:
        return hash((self.name, self.version))

    @override
    def __str__(self) -> str:
        return f"SolverPackage\nname: {self.name!s}\nversion: {self.version!s}\n"


@dataclass
class SolverPackageWithFlag:
    package: SolverPackage
    flag: str | NoFlags

    @override
    def __eq__(self, other: object) -> bool:
        if not isinstance(other, SolverPackageWithFlag):
            return False
        return self.package == other.package and self.flag == other.flag

    @override
    def __hash__(self) -> int:
        return hash((self.package, self.flag))

    @override
    def __str__(self) -> str:
        return f"SolverPackageWithFlag\nname: {self.package.name!s}\nversion: {self.package.version!s}\nflag: {self.flag!s}"
