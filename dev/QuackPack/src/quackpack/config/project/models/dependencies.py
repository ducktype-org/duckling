from typing import Annotated, Any, Final

from pydantic import BaseModel, Discriminator, Field, StrictStr, Tag, model_validator
from pydantic_core import InitErrorDetails, ValidationError

from quackpack.config.project.models.dependency_conditions import DependencyConditions
from quackpack.config.project.models.git_entry import GitEntry
from quackpack.config.project.models.local_entry import LocalEntry
from quackpack.config.project.models.version_list import VersionList, VersionListValueError
from quackpack.util.default_pydantic_options import default_pydantic_options
from quackpack.util.pydantic_helpers import PydanticMutableDict


class DependencyEntry(BaseModel):
    """
    Entry of a single dependency.
    """

    _VERSION_OPTION_VERSION_LIST: Final[str] = "!version_option_version_list"
    _VERSION_OPTION_GIT: Final[str] = "!version_option_git"
    _VERSION_OPTION_LOC: Final[str] = "!version_option_loc"
    _FLAGS_OPTION_STR: Final[str] = "!flags_option_str"
    _FLAGS_OPTION_DICT: Final[str] = "!flags_option_dict"

    # NOTE: These need to be @staticmethod, because they serve as tag discriminants.
    @staticmethod
    def _version_discriminator(v: Any) -> str | None:
        if isinstance(v, VersionList):
            return DependencyEntry._VERSION_OPTION_VERSION_LIST
        if isinstance(v, GitEntry) or (isinstance(v, dict) and "git_url" in v):
            return DependencyEntry._VERSION_OPTION_GIT
        if isinstance(v, LocalEntry) or (isinstance(v, dict) and "path" in v):
            return DependencyEntry._VERSION_OPTION_LOC
        return None

    @staticmethod
    def _flags_discriminator(v: Any) -> str | None:
        if isinstance(v, str):
            return DependencyEntry._FLAGS_OPTION_STR
        if isinstance(v, dict):
            return DependencyEntry._FLAGS_OPTION_DICT
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
    conditions: DependencyConditions = Field(default_factory=DependencyConditions)
    """
    Optional conditions required for enabling this dependency.
    """
    flags: list[
        Annotated[
            (
                Annotated[StrictStr, Tag(_FLAGS_OPTION_STR)]
                | Annotated[dict[StrictStr, DependencyConditions], Tag(_FLAGS_OPTION_DICT)]
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

    @model_validator(mode="before")
    @classmethod
    def _convert_version(cls, values: Any) -> Any:
        if not isinstance(values, dict):
            return values
        if "version" not in values:
            return values  # pyright: ignore [reportUnknownVariableType]
        maybe_versions = values["version"]  # pyright: ignore [reportUnknownVariableType]
        if not isinstance(maybe_versions, (str, list)):
            return values  # pyright: ignore [reportUnknownVariableType]
        try:
            values["version"] = VersionList.from_raw_data(maybe_versions)  # pyright: ignore [reportUnknownArgumentType]
        # We raise custom ValidationErrors to provide a more precise error location.
        except VersionListValueError as exc:
            error_details = InitErrorDetails(
                type="value_error", loc=("version", exc.index), input=None, ctx={"error": exc}
            )
            raise ValidationError.from_exception_data(
                title="Value error", line_errors=[error_details]
            ) from None
        except ValueError as exc:
            if isinstance(exc, ValidationError):
                # This happens if _check_nonempty_versions fails.
                exc_details = exc.errors()[0]
                context = exc_details["ctx"] if "ctx" in exc_details else {"error": exc_details["msg"]}
                error_details = InitErrorDetails(
                    type="value_error", loc=("version",), input=None, ctx=context
                )
            else:
                error_details = InitErrorDetails(
                    type="value_error", loc=("version",), input=None, ctx={"error": exc}
                )
            raise ValidationError.from_exception_data(
                title="Value error", line_errors=[error_details]
            ) from None
        return values  # pyright: ignore [reportUnknownVariableType]

    model_config = default_pydantic_options()


class Dependencies(PydanticMutableDict[StrictStr, DependencyEntry]):
    """
    Map with all of the dependencies.
    """
