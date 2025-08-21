from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version

from .source import Source


class DependencySpec:
    def __init__(self, manifest_name: Identifier, versions: Version | list[Version], source: Source):
        self._manifest_name = manifest_name
        self._source = source
        if isinstance(versions, Version):
            self._versions = [versions]
        else:
            self._versions = versions

    @property
    def manifest_name(self) -> Identifier:
        return self._manifest_name

    @property
    def source(self) -> Source:
        return self._source

    @property
    def versions(self) -> list[Version]:
        return self._versions
