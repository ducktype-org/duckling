# TODO
# Use this as package names for types like SolverPackage and SolverPackageWithFlag.
# This name should be different for all different git and local packages and packages from different servers.
# Two versions of the same package from some server should have the same name.
from __future__ import annotations

from pathlib import Path
from typing import override

from pydantic import AnyUrl

from quackpack.config.project import DependencyEntry, GitEntry, LocalEntry, VersionList
from quackpack.util.pkgid import Identifier

DEFAULT_SERVER_URL = AnyUrl("http://localhost:9001")

NO_NAME = Identifier("NONE")


class UniversalName:
    name: Identifier
    server_url: AnyUrl | None
    local_path: Path | None
    git_entry: GitEntry | None

    def __init__(
        self,
        name: Identifier = NO_NAME,
        server_url: AnyUrl | None = None,
        local_path: Path | None = None,
        git_entry: GitEntry | None = None,
    ):
        self.name = name
        self.server_url = server_url
        self.local_path = local_path
        self.git_entry = git_entry

    def is_server(self):
        return self.server_url is not None

    def is_local(self):
        return self.local_path is not None

    def is_git(self):
        return self.git_entry is not None

    @override
    def __eq__(self, other: object) -> bool:
        if not isinstance(other, UniversalName):
            return False
        return (
            self.name == other.name
            and self.server_url == other.server_url
            and self.local_path == other.local_path
            and self.git_entry == other.git_entry
        )

    @override
    def __hash__(self) -> int:
        return hash((self.name, self.server_url, self.local_path, self.git_entry))

    @classmethod
    def server_uni_name(cls, name: Identifier, server_url: AnyUrl) -> UniversalName:
        return UniversalName(name=name, server_url=server_url)

    @classmethod
    def local_uni_name(cls, local_path: Path) -> UniversalName:
        return UniversalName(local_path=local_path)

    @classmethod
    def git_uni_name(cls, git_entry: GitEntry) -> UniversalName:
        return UniversalName(git_entry=git_entry)

    @classmethod
    def create_from_dep_entry(cls, dep_name: Identifier, dep_entry: DependencyEntry) -> UniversalName:
        if isinstance(dep_entry.version, LocalEntry):
            return cls.local_uni_name(dep_entry.version.path)
        elif isinstance(dep_entry.version, VersionList):
            return cls.server_uni_name(
                name=dep_name,
                server_url=DEFAULT_SERVER_URL if dep_entry.server_url is None else dep_entry.server_url,
            )
        else:
            return cls.git_uni_name(dep_entry.version)
