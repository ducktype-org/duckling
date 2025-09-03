from __future__ import annotations

from quackpack.core.types.manifest.schemas.registry import (
    RegistryManifestSchema,
    RegistryMetadataSchema,
    RegistrySemverSchema,
)
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version

from .compiler_options import Profiles, Targets
from .dependencies import Dependencies
from .features import Features
from .root_spec import RootSpec


class Summary:
    def __init__(
        self,
        spec: RootSpec,
        features: Features,
        authors: list[str] | None,
        license_: str | None,
        description: str | None,
        dependencies: Dependencies,
        dev_dependencies: Dependencies,
        profiles: Profiles,
        targets: Targets,
    ):
        self._spec = spec
        self._features = features
        self._authors = authors
        self._license = license_
        self._description = description
        self._dependencies = dependencies
        self._dev_dependendcies = dev_dependencies
        self._profiles = profiles
        self._targets = targets

    @property
    def spec(self) -> RootSpec:
        return self._spec

    @property
    def name(self) -> Identifier:
        return self.spec.name

    @property
    def version(self) -> Version:
        return self.spec.version

    @property
    def features(self) -> Features:
        return self._features

    @property
    def deps(self) -> Dependencies:
        return self._dependencies

    @property
    def dev_deps(self) -> Dependencies:
        return self._dev_dependendcies

    @property
    def profiles(self) -> Profiles:
        return self._profiles

    @property
    def targets(self) -> Targets:
        return self._targets

    @property
    def authors(self) -> list[str] | None:
        return self._authors

    @property
    def license(self) -> str | None:
        return self._license

    @property
    def description(self) -> str | None:
        return self._description

    def into_schema(self) -> RegistryManifestSchema:
        metadata = RegistryMetadataSchema(
            version=RegistrySemverSchema(str(self.version)),
            authors=self.authors or [],
            license=self.license or "",
            name=str(self.name),
            description=self.description or "",
        )
        deps = self.deps.into_schema()
        dev_deps = self.dev_deps.into_schema()
        features = self.features.into_schema()
        targets = self.targets.into_schema()
        profiles = self.profiles.into_schema()
        return RegistryManifestSchema(
            metadata=metadata,
            dependencies=deps,
            dev_dependencies=dev_deps,
            features=features,
            targets=targets,
            profiles=profiles,
        )

    @classmethod
    def from_schema(cls, schema: RegistryManifestSchema) -> Summary:
        version = Version.create_from_string(schema.metadata.version.root)
        name = Identifier(schema.metadata.name)
        features = Features.from_schema(schema.features)
        deps = Dependencies.from_schema(schema.dependencies)
        dev_deps = Dependencies.from_schema(schema.dev_dependencies)
        targets = Targets.from_schema(schema.targets)
        profiles = Profiles.from_schema(schema.profiles)
        return Summary(
            RootSpec(name, version),
            features,
            schema.metadata.authors,
            schema.metadata.license,
            schema.metadata.description,
            deps,
            dev_deps,
            profiles,
            targets,
        )
