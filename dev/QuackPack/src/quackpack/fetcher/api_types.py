from dataclasses import dataclass
from pathlib import Path

from pydantic import AnyUrl, BaseModel

from quackpack.config.project import Manifest
from quackpack.config.project.git_entry import GitEntry
from quackpack.util.pkgid import Identifier

__all__ = ["MultiMetadata", "MultiMetadataResult", "Package", "SingleMetadata", "SingleMetadataResult"]

SingleMetadata = Manifest

# TODO: actual implement this in pending merge request
PackageName = Identifier
URLType = AnyUrl


class MultiMetadata(BaseModel):
    packages_metadata: list[SingleMetadata]


class Package(BaseModel):
    id: PackageName
    # TODO: find something better
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
    git_entry: GitEntry
    destination_path: Path
    commit_hash: str
    result: SingleMetadata | None


@dataclass(frozen=True)
class SearchResult(BaseModel):
    result: list[Package]
