from __future__ import annotations

from collections.abc import ItemsView, Iterable, Iterator, KeysView, ValuesView

from pydantic import RootModel

from quackpack.core.types.manifest.schemas.registry import FeaturesSchema
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier


class Features:
    def __init__(self, features: dict[Identifier, list[Identifier]]):
        self._impl = features
        self._check()

    def _check(self):
        self._check_valid_values()
        # TODO: Check for cycles?

    def _check_valid_values(self):
        for root, features in self._impl.items():
            for feature in features:
                if feature not in self._impl:
                    raise QuackPackError(
                        f"feature '{root}' requires feature '{feature}', but it's not present"
                    )

    def has_feature(self, feature: Identifier) -> bool:
        return feature in self._impl

    def expand_features(self, features: Iterable[Identifier]) -> list[Identifier]:
        already_visited_features: set[Identifier] = set()
        # Używamy list, żeby zachować kolejność.
        current_stack = list(features)
        result: list[Identifier] = []
        while current_stack:
            feature = current_stack.pop(0)
            if feature in already_visited_features:
                continue
            if feature not in self._impl:
                raise QuackPackError(f"there is no such feature as '{feature}'")
            extra = [
                new_feature
                for new_feature in self._impl[feature]
                if new_feature not in already_visited_features
            ]
            result.append(feature)
            already_visited_features.add(feature)
            current_stack += extra
        return result

    def expand_feature(self, feature: Identifier) -> list[Identifier]:
        return self.expand_features((feature,))

    def __iter__(self) -> Iterator[Identifier]:
        return iter(self._impl)

    def keys(self) -> KeysView[Identifier]:
        return self._impl.keys()

    def values(self) -> ValuesView[list[Identifier]]:
        return self._impl.values()

    def items(self) -> ItemsView[Identifier, list[Identifier]]:
        return self._impl.items()

    def __contains__(self, o: object) -> bool:
        if not isinstance(o, Identifier):
            return False
        return self.has_feature(o)

    def __getitem__(self, key: Identifier) -> list[Identifier]:
        return self._impl[key]

    def __len__(self) -> int:
        return len(self._impl)

    def into_schema(self) -> FeaturesSchema:
        impl: dict[str, list[str]] = {}
        for k, v in self.items():
            x = [str(x) for x in v]
            impl[str(k)] = x
        return RootModel(impl)

    @classmethod
    def from_schema(cls, schema: FeaturesSchema) -> Features:
        impl: dict[Identifier, list[Identifier]] = {}
        for k, v in schema.root.items():
            x = [Identifier(x) for x in v]
            impl[Identifier(k)] = x
        return Features(impl)
