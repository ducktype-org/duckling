from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version


class RootSpec:
    def __init__(self, name: Identifier, version: Version):
        self._name = name
        self._version = version

    @property
    def name(self) -> Identifier:
        return self._name

    @property
    def version(self) -> Version:
        return self._version
