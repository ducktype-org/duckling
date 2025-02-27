from pathlib import Path
from typing import Annotated, Any

from pydantic import BaseModel, Discriminator, RootModel, Tag, model_validator

from .version import Version, VersionList

SingleMetadata = Any
MultiMetadata = Any


# TODO: this is lightweight Package class for fetcher (and probably api) purposes
# TODO: add source of package
class Package(BaseModel):
    name: str
    version: Version


# TODO: in this class add source of the package
class Metadata(BaseModel):
    """
    Global project metadata.
    """

    author: str
    """
    Author of the project.
    """
    version: Version
    """
    Version of the project.
    """
    id: str  # TODO: Change to validated (different) type.
    """
    Internal project ID.
    """
    name: str
    """
    Name of the project.
    """
    license: str
    """
    License used in the project.
    """
    description: str | None = None
    """
    Description of the project.
    """
    is_temporary: bool = (
        False  # TODO: If we want to use different name in configuration file, then alias it here.
    )
    """
    If `True`, then this project uses temporary venv.
    """
    compiler_version: Version | None = None
    """
    Version of compiler required for that project.
    """
    vm_version: Version | None = None
    """
    Version of VM required for that project.
    """

    @model_validator(mode="before")
    @classmethod
    def convert_version(cls, values: Any) -> Any:
        if not isinstance(values, dict):
            return values
        for s in ("version", "compiler_version", "vm_version"):
            if s in values and isinstance(values[s], str):
                values[s] = Version.create_from_string(values[s])  # pyright: ignore [reportUnknownArgumentType], we just checked, that it's str.
        return values  # pyright: ignore [reportUnknownVariableType]


# TODO: add discriminators for all of the unions
class DependencyConditions(BaseModel):
    """
    Optional conditions of the dependency.
    """

    system: str | list[str] | None = None
    """
    If not `None`, then match host system against provided system(s).
    """
    arch: str | list[str] | None = None
    """
    If not `None`, then match host CPU architecture against provided architecture(s).
    """
    project_flags: str | list[str] | None = None  # TODO: Convert type to logic string.
    """
    If not `None`, then enable this dependency only if we are building project with any of the `project_flags`.
    """


# TODO: What actually we need?
class GitEntry(BaseModel):
    """
    Git source of the dependency.
    """

    git_url: str
    """
    Git url.
    """
    commit: str | None = None
    """
    Git commit.
    """
    tag: str | None = None
    """
    Git tag.
    """
    branch: str | None = None
    """
    Git branch.
    """

    @model_validator(mode="before")
    @classmethod
    def check_mutually_exclusive_fields(cls, values: Any) -> Any:
        if not isinstance(values, dict):
            return values
        keys_in_dict = sum(1 for key in ("commit", "tag", "branch") if key in values)
        if keys_in_dict > 1:
            raise ValueError("Expected at most one of 'commit', 'tag', 'branch'")
        return values  # pyright: ignore [reportUnknownVariableType]


class LocalEntry(BaseModel):
    """
    Local source of the dependency.
    """

    path: Path
    """
    Local path to the dependency.
    """


VERSION_OPTION_VERSION_LIST = "!version_option_version_list"
VERSION_OPTION_GIT = "!version_option_git"
VERSION_OPTION_LOC = "!version_option_loc"


def version_discriminator(v: Any) -> str | None:
    if isinstance(v, (list, str)):
        return VERSION_OPTION_VERSION_LIST
    if isinstance(v, dict) and "git_url" in v:
        return VERSION_OPTION_GIT
    if isinstance(v, dict) and "path" in v:
        return VERSION_OPTION_LOC
    return None


FLAGS_OPTION_STR = "!flags_option_str"
FLAGS_OPTION_DICT = "!flags_option_dict"


def flags_key_discriminator(v: Any) -> str | None:
    if isinstance(v, str):
        return FLAGS_OPTION_STR
    if isinstance(v, dict):
        return FLAGS_OPTION_DICT
    return None


class DependencyEntry(BaseModel):
    """
    Entry of a single dependency.
    """

    version: Annotated[
        (
            Annotated[VersionList, Tag(VERSION_OPTION_VERSION_LIST)]
            | Annotated[GitEntry, Tag(VERSION_OPTION_GIT)]
            | Annotated[LocalEntry, Tag(VERSION_OPTION_LOC)]
        ),
        Discriminator(
            version_discriminator,
            custom_error_type="invalid_union_member",
            custom_error_message=("Value should be a string, list of strings, GitEntry or LocalEntry"),
        ),
    ]
    """
    Dependency version from repository, or third-party location.
    """
    conditions: DependencyConditions | None = None
    """
    Optional conditions required for enabling this dependency.
    """
    flags: (
        list[
            Annotated[
                (
                    Annotated[str, Tag(FLAGS_OPTION_STR)]
                    | Annotated[dict[str, DependencyConditions], Tag(FLAGS_OPTION_DICT)]
                ),
                Discriminator(
                    flags_key_discriminator,
                    custom_error_type="invalid_union_member",
                    custom_error_message=(
                        "Value should be a list of strings or mappings from strings to DependencyConditions"
                    ),
                ),
            ]
        ]
        | None
    ) = None
    """
    Build dependency with specified flags.
    """


type Dependencies = RootModel[dict[str, list[DependencyEntry]]]
"""
Map with all of the dependencies.
"""


# TODO: Add rest of missing fields.
class Configuration(BaseModel):
    """
    Full project configuration.
    """

    metadata: Metadata
    """
    Project metadata.
    """
    dependencies: Dependencies | None = None
    """
    Project dependencies.
    """
