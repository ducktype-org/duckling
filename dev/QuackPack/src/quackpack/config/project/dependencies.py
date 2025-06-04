from typing import Annotated, Any, Final, Self, cast

from pydantic import AnyUrl, BaseModel, Discriminator, Field, Tag, field_validator, model_validator
from pydantic_core import InitErrorDetails, ValidationError

from quackpack.config.project.dependency_conditions import DependencyConditions
from quackpack.config.project.git_entry import GitEntry
from quackpack.config.project.local_entry import LocalEntry
from quackpack.config.project.version_list import VersionList, VersionListValueError
from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.pkgid import Identifier
from quackpack.util.pydantic_helpers import PydanticMutableDict

_VERSION_OPTION_VERSION_LIST: Final[str] = "!version_option_version_list"
_VERSION_OPTION_GIT: Final[str] = "!version_option_git"
_VERSION_OPTION_LOC: Final[str] = "!version_option_loc"
_FLAGS_OPTION_STR: Final[str] = "!flags_option_str"
_FLAGS_OPTION_DICT: Final[str] = "!flags_option_dict"


class ServerUrlShouldBeBlank(ValueError):
    def __init__(self, message: str):
        super().__init__(message)


class DependencyEntry(BaseModel):
    """
    Entry of a single dependency.
    """

    # NOTE: These need to be @staticmethod, because they serve as tag discriminants.
    @staticmethod
    def _version_discriminator(v: Any) -> str | None:
        if isinstance(v, VersionList):
            return _VERSION_OPTION_VERSION_LIST
        if isinstance(v, GitEntry) or (isinstance(v, dict) and "git_url" in v):
            return _VERSION_OPTION_GIT
        if isinstance(v, LocalEntry) or (isinstance(v, dict) and "path" in v):
            return _VERSION_OPTION_LOC
        return None

    @staticmethod
    def _flags_discriminator(v: Any) -> str | None:
        if isinstance(v, (str, Identifier)):
            return _FLAGS_OPTION_STR
        if isinstance(v, dict):
            return _FLAGS_OPTION_DICT
        return None

    version: Annotated[
        (
            Annotated[VersionList, Tag(_VERSION_OPTION_VERSION_LIST)]
            | Annotated[GitEntry, Tag(_VERSION_OPTION_GIT)]
            | Annotated[LocalEntry, Tag(_VERSION_OPTION_LOC)]
        ),
        Discriminator(
            _version_discriminator,
            custom_error_type="invalid_union_member",
            custom_error_message="Value should be a string, list of strings, GitEntry or LocalEntry",
        ),
    ]
    """
    Dependency version from repository, or third-party location.
    """

    server_url: AnyUrl | None = None

    conditions: DependencyConditions = Field(default_factory=DependencyConditions)
    """
    Optional conditions required for enabling this dependency.
    """

    # FIXME: Czy pozwalamy na parę kluczy w opcji `dict[...]`?
    flags: list[
        Annotated[
            (
                Annotated[Identifier, Tag(_FLAGS_OPTION_STR)]
                | Annotated[dict[Identifier, DependencyConditions], Tag(_FLAGS_OPTION_DICT)]
            ),
            Discriminator(
                _flags_discriminator,
                custom_error_type="invalid_union_member",
                custom_error_message=(
                    "Value should be a list of strings or mappings from strings to DependencyConditions"
                ),
            ),
        ]
    ] = []
    """
    Build dependency with specified flags.
    """

    @field_validator("version", mode="before")
    @classmethod
    def _convert_version(cls, value: Any) -> Any:
        if not isinstance(value, (str, list)):
            return value
        try:
            return VersionList.from_raw_data(cast(str | list[Any], value))
        # NOTE: Since we moved from model_validator to field_validator, we need to remove "version" from InitErrorDetails.loc.
        except VersionListValueError as exc:
            error_details = InitErrorDetails(
                type="value_error", loc=(exc.index,), input=None, ctx={"error": exc}
            )
            raise ValidationError.from_exception_data(
                title="Value error", line_errors=[error_details]
            ) from None
        except ValueError as exc:
            if isinstance(exc, ValidationError):
                # This happens if _check_nonempty_versions fails.
                exc_details = exc.errors()[0]
                context = exc_details["ctx"] if "ctx" in exc_details else {"error": exc_details["msg"]}
                error_details = InitErrorDetails(type="value_error", input=None, ctx=context)
            else:
                error_details = InitErrorDetails(type="value_error", input=None, ctx={"error": exc})
            raise ValidationError.from_exception_data(
                title="Value error", line_errors=[error_details]
            ) from None

    @model_validator(mode="after")
    def check_server_url(self) -> Self:
        if isinstance(self.version, (GitEntry, LocalEntry)) and self.server_url is not None:
            raise ServerUrlShouldBeBlank(
                "The server_url field should be blank, as the dependency is not a server dependency."
            )
        return self

    model_config = DEFAULT_MODEL_CONFIG


class Dependencies(PydanticMutableDict[Identifier, DependencyEntry]):
    """
    Map with all the dependencies.
    """
