from collections.abc import ItemsView, Iterator, KeysView, ValuesView

from quackpack.core.types.manifest.dependency_spec import DependencySpec
from quackpack.core.types.manifest.schemas.registry import RegistryDependencySchema
from quackpack.core.types.manifest.source import Source
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version

from .dependency import Dependency


class Dependencies:
    def __init__(self, deps: dict[Identifier, Dependency]):
        self._impl = deps

    def has_dependency(self, name: Identifier) -> bool:
        return name in self._impl

    def get_dependency(self, name: Identifier) -> Dependency:
        return self._impl[name]

    def __iter__(self) -> Iterator[Identifier]:
        return iter(self._impl)

    def keys(self) -> KeysView[Identifier]:
        return self._impl.keys()

    def values(self) -> ValuesView[Dependency]:
        return self._impl.values()

    def items(self) -> ItemsView[Identifier, Dependency]:
        return self._impl.items()

    def __contains__(self, o: object) -> bool:
        if not isinstance(o, Identifier):
            return False
        return self.has_dependency(o)

    def __getitem__(self, key: Identifier) -> Dependency:
        return self._impl[key]

    def __len__(self) -> int:
        return len(self._impl)

    def into_schema(self) -> dict[str, RegistryDependencySchema]:
        return {str(k): v.into_schema() for k, v in self.items()}

    @classmethod
    def from_schema(cls, schema: dict[str, RegistryDependencySchema]) -> Dependencies:
        impl: dict[Identifier, Dependency] = {}
        for k, v in schema.items():
            name = Identifier(k)
            versions = [Version.create_from_string(x.root) for x in v.version]
            source = Source.from_schema(v.source)
            spec = DependencySpec(name, versions, source)
            impl[name] = Dependency.from_schema(v, spec)
        return Dependencies(impl)
