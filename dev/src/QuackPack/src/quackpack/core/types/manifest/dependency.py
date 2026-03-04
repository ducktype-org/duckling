import platform
from collections.abc import Iterable
from typing import Any

from pydantic import RootModel

from quackpack.core.types.manifest.schemas.registry import (
    RegistryDependencyConditionSchema,
    RegistryDependencySchema,
    RegistryDetailedFeatureSchema,
    RegistrySemverSchema,
)
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version

from .dependency_spec import DependencySpec
from .source import Source


class Conditions:
    def __init__(
        self,
        system_requirements: list[str] | None,
        cpu_requirements: list[str] | None,
        required_package_features: list[Identifier] | None,
    ):
        self._system = system_requirements
        self._cpu = cpu_requirements
        self._features = required_package_features

        def _check_non_empty(x: Iterable[Any] | None):
            if x is not None and not x:
                raise Exception("empty iterator")

        _check_non_empty(self._features)
        _check_non_empty(self._cpu)
        _check_non_empty(self._system)

    def is_enabled(self, enabled_features: Iterable[Identifier] | Identifier) -> bool:
        return (
            self._is_system_ok()
            and self._is_cpu_ok()
            and self._are_features_ok(enabled_features)
        )

    def _is_system_ok(self) -> bool:
        # NOTE: This uses Python's classification of the systems, were DuckType to introduce a new one, this should be adapted accordingly.
        if self._system is None:
            return True
        return platform.system().lower() in (system.lower() for system in self._system)

    def _is_cpu_ok(self) -> bool:
        # NOTE: This uses Python's classification of the CPUs, were DuckType to introduce a new one, this should be adapted accordingly.
        if self._cpu is None:
            return True
        return platform.machine().lower() in (cpu.lower() for cpu in self._cpu)

    def _are_features_ok(
        self, enabled_features: Iterable[Identifier] | Identifier
    ) -> bool:
        if self._features is None:
            return True
        to_iter = (
            (enabled_features,)
            if isinstance(enabled_features, Identifier)
            else enabled_features
        )
        return any(feature in self._features for feature in to_iter)

    def into_schema(self) -> RegistryDependencyConditionSchema:
        return RegistryDependencyConditionSchema(
            system=self._system,
            arch=self._cpu,
            package_features=(
                [str(x) for x in self._features] if self._features is not None else None
            ),
        )

    @classmethod
    def from_schema(cls, schema: RegistryDependencyConditionSchema) -> Conditions:
        return Conditions(
            schema.system,
            schema.arch,
            (
                [Identifier(x) for x in schema.package_features]
                if schema.package_features is not None
                else None
            ),
        )


class DependencyFeature:
    def __init__(self, name: Identifier, conditions: Conditions | None):
        self._name = name
        self._conditions = conditions

    @property
    def name(self) -> Identifier:
        return self._name

    def is_enabled(self, enabled_features: Iterable[Identifier]) -> bool:
        if self._conditions is None:
            return True
        return self._conditions.is_enabled(enabled_features)

    def into_schema(self) -> str | RegistryDetailedFeatureSchema:
        if self._conditions is None:
            return str(self.name)
        return RootModel({str(self.name): self._conditions.into_schema()})

    @classmethod
    def from_schema(
        cls, schema: str | RegistryDetailedFeatureSchema
    ) -> DependencyFeature:
        if isinstance(schema, str):
            return DependencyFeature(Identifier(schema), None)
        root = schema.root
        assert len(root) == 1, "pydantic?"
        for k, v in root.items():
            return DependencyFeature(Identifier(k), Conditions.from_schema(v))
        assert False


class Dependency:
    def __init__(
        self,
        spec: DependencySpec,
        features: list[DependencyFeature],
        is_pinned: bool,
        conditions: Conditions | None,
        real_name: Identifier | None,
    ):
        self._spec = spec
        self._features = features
        self._pinned = is_pinned
        self._conditions = conditions
        self._real_name = real_name

    @property
    def spec(self) -> DependencySpec:
        return self._spec

    @property
    def manifest_name(self) -> Identifier:
        return self.spec.manifest_name

    @property
    def source(self) -> Source:
        return self.spec.source

    @property
    def versions(self) -> list[Version]:
        return self.spec.versions

    @property
    def real_name(self) -> Identifier:
        return self._real_name or self.manifest_name

    @property
    def is_pinned(self) -> bool:
        return self._pinned

    def is_enabled(self, enabled_features: Iterable[Identifier] | Identifier) -> bool:
        if self._conditions is None:
            return True
        return self._conditions.is_enabled(enabled_features)

    @property
    def features(self) -> list[DependencyFeature]:
        return self._features

    def enabled_features(
        self, enabled_features: Iterable[Identifier] | Identifier
    ) -> Iterable[Identifier]:
        to_iter = (
            (enabled_features,)
            if isinstance(enabled_features, Identifier)
            else enabled_features
        )
        return (
            feature.name for feature in self._features if feature.is_enabled(to_iter)
        )

    def into_schema(self) -> RegistryDependencySchema:
        return RegistryDependencySchema(
            version=[RegistrySemverSchema(str(x)) for x in self.versions],
            source=self.source.into_schema(),
            features=[x.into_schema() for x in self.features],
            pinned=self.is_pinned,
            conditions=(
                self._conditions.into_schema()
                if self._conditions is not None
                else RegistryDependencyConditionSchema(
                    system=None, arch=None, package_features=None
                )
            ),
            is_alias_for=str(self._real_name) if self._real_name is not None else None,
        )

    @classmethod
    def from_schema(
        cls, schema: RegistryDependencySchema, spec: DependencySpec
    ) -> Dependency:
        features = [DependencyFeature.from_schema(x) for x in schema.features]
        is_pinned = schema.pinned
        conditions = Conditions.from_schema(schema.conditions)
        real_name = (
            Identifier(schema.is_alias_for) if schema.is_alias_for is not None else None
        )
        return Dependency(spec, features, is_pinned, conditions, real_name)
