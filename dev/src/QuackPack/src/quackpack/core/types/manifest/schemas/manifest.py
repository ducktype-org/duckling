from __future__ import annotations

from collections.abc import Iterable
from pathlib import Path

from pydantic import BaseModel, ConfigDict, RootModel, field_serializer, field_validator


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


class SemverSchema(RootModel[str]):
    @field_validator("root", mode="after")
    @classmethod
    def _check(cls, root: str) -> str:
        _check_valid_semver(root)
        return root


type DucklingVersionSchema = SemverSchema


class MetadataSchema(BaseModel):
    version: SemverSchema | None = None
    authors: list[str] | None = None
    license: str | None = None
    name: str | None = None
    description: str | None = None
    language_version: DucklingVersionSchema | None = None

    model_config = ConfigDict(extra="allow")


class DependencyConditionSchema(BaseModel):
    system: list[str] | None = None
    arch: list[str] | None = None
    package_features: list[str] | None = None

    model_config = ConfigDict(extra="allow")


type DetailedFeatureSchema = RootModel[dict[str, DependencyConditionSchema]]


class OredSemverSchema(RootModel[str]):
    @field_validator("root", mode="after")
    @classmethod
    def _check(cls, root: str) -> str:
        for version in OredSemverSchema._make_split_impl(root):
            _check_valid_semver(version)
        return root

    @classmethod
    def _make_split_impl(cls, input: str) -> Iterable[str]:
        return (x.strip() for x in input.split(" or "))

    def split_versions(self) -> Iterable[str]:
        return OredSemverSchema._make_split_impl(self.root)


type VersionSchema = list[SemverSchema] | OredSemverSchema

type SimpleSourceSchema = str


class SourceSchema(BaseModel):
    registry_url: str | None = None
    name: str | None = None
    path: Path | None = None
    git_url: str | None = None
    tag: str | None = None
    commit: str | None = None
    branch: str | None = None

    @classmethod
    def of_git(cls, git_url: str) -> SourceSchema:
        return SourceSchema(git_url=git_url)

    @classmethod
    def of_path(cls, path: Path) -> SourceSchema:
        return SourceSchema(path=path)

    @field_serializer("path")
    def _as_posix(self, p: Path | None) -> str | None:
        return p.as_posix() if p is not None else None

    model_config = ConfigDict(extra="allow")


class DependencySchema(BaseModel):
    version: VersionSchema | None = None
    source: SimpleSourceSchema | SourceSchema | None = None
    features: list[str | DetailedFeatureSchema] | None = None
    pinned: bool | None = None
    conditions: DependencyConditionSchema | None = None

    model_config = ConfigDict(extra="allow")


type FeaturesSchema = RootModel[dict[str, list[str]]]


class CompilerOptionsSchema(BaseModel):
    compiler_flags: list[str] | None = None

    model_config = ConfigDict(extra="allow")


type ProfileSchema = RootModel[dict[str, CompilerOptionsSchema]]


class ManifestSchema(BaseModel):
    metadata: MetadataSchema | None = None
    dependencies: dict[str, DependencySchema] | None = None
    dev_dependencies: dict[str, DependencySchema] | None = None
    features: FeaturesSchema | None = None
    targets: ProfileSchema | None = None
    profiles: ProfileSchema | None = None

    model_config = ConfigDict(extra="allow")
