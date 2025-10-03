"""
Module containing types used in fetcher code.
"""

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from pydantic import BaseModel

from quackpack.core.types.manifest.schemas.registry import RegistryManifestSchema
from quackpack.util.types.pkgid import Identifier

__all__ = [
    "MultiMetadata",
    "MultiMetadataResult",
    "Package",
    "SingleMetadata",
    "SingleMetadataResult",
]

SingleMetadata = RegistryManifestSchema

PackageName = Identifier
URLType = str


class MultiMetadata(BaseModel):
    packages_metadata: list[SingleMetadata]


class Package(BaseModel):
    id: PackageName
    version: str


@dataclass(frozen=True)
class SingleMetadataResult:
    package: Package
    instance_url: URLType
    result: SingleMetadata | None


@dataclass(frozen=True)
class MultiMetadataResult:
    package_name: PackageName
    instance_url: URLType
    result: MultiMetadata | None


@dataclass(frozen=True)
class SingleGitMetadataResult:
    git_entry: Any
    destination_path: Path
    commit_hash: str
    result: SingleMetadata | None


@dataclass(frozen=True)
class SearchResult(BaseModel):
    result: list[Package]
