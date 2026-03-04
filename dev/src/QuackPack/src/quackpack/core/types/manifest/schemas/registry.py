"""
NOTE: This schemas should be the same as ducknest/utils/models.py schemas.
"""

from pathlib import Path
from typing import Literal

from pydantic import BaseModel, Field, RootModel, field_serializer, field_validator


def _check_valid_semver(input: str) -> None:
    if not input:
        raise ValueError("empty string is not a valid version")
    parts = input.split(".")
    if len(parts) > 3:
        raise ValueError("expected at most three dots in a version")
    names = ("major", "minor", "patch")
    all_zeros = True
    for idx, (part, name) in enumerate(zip(parts, names, strict=False)):
        try:
            as_int = int(part)
        except ValueError:
            raise ValueError(
                f"{name} version is not a valid integer: got `{part}`"
            ) from None
        if as_int < 0:
            raise ValueError(f"{name} version is negative")
        # Skip patch.
        if as_int != 0 and idx != 2:
            all_zeros = False
    if all_zeros:
        raise ValueError("versions of format `0.0.X` are not supported")


class RegistrySemverSchema(RootModel[str]):
    @field_validator("root", mode="after")
    @classmethod
    def _check(cls, root: str) -> str:
        _check_valid_semver(root)
        return root


type DucklingVersionSchema = RegistrySemverSchema


class RegistryMetadataSchema(BaseModel):
    version: RegistrySemverSchema
    authors: list[str]
    license: str
    name: str
    description: str


class RegistryDependencyConditionSchema(BaseModel):
    system: list[str] | None = None
    arch: list[str] | None = None
    package_features: list[str] | None = None


type RegistryDetailedFeatureSchema = RootModel[
    dict[str, RegistryDependencyConditionSchema]
]


class RegistrySourceSchema(BaseModel):
    inner: DefaultSourceSchema | LocalSourceSchema | GitSourceSchema = Field(
        ..., discriminator="type"
    )


class DefaultSourceSchema(BaseModel):
    registry_url: str
    type: Literal["registry"] = "registry"


class LocalSourceSchema(BaseModel):
    absolute_dir_root: Path
    dir_entry_in_manifest: Path
    type: Literal["local"] = "local"

    @field_serializer("absolute_dir_root", "dir_entry_in_manifest")
    def _as_posix(self, p: Path) -> str:
        return p.as_posix()


class GitSourceSchema(BaseModel):
    type: Literal["git"] = "git"
    git_url: str
    commit: str | None = None
    tag: str | None = None
    branch: str | None = None


class RegistryDependencySchema(BaseModel):
    version: list[RegistrySemverSchema]
    source: RegistrySourceSchema
    features: list[str | RegistryDetailedFeatureSchema]
    pinned: bool
    conditions: RegistryDependencyConditionSchema
    is_alias_for: str | None = None


type FeaturesSchema = RootModel[dict[str, list[str]]]


class CompilerOptionsSchema(BaseModel):
    compiler_flags: list[str]


type ProfileSchema = RootModel[dict[str, CompilerOptionsSchema]]


class RegistryManifestSchema(BaseModel):
    metadata: RegistryMetadataSchema
    dependencies: dict[str, RegistryDependencySchema]
    dev_dependencies: dict[str, RegistryDependencySchema]
    features: FeaturesSchema
    targets: ProfileSchema
    profiles: ProfileSchema
