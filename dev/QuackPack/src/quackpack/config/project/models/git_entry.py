from typing import Any

from pydantic import AnyUrl, BaseModel, StrictStr, field_serializer, model_validator

from quackpack.util.default_pydantic_options import default_pydantic_options


# TODO: What actually we need?
class GitEntry(BaseModel):
    """
    Git source of the dependency.
    """

    git_url: AnyUrl
    """
    Git url.
    """
    commit: StrictStr | None = None
    """
    Git commit.
    """
    tag: StrictStr | None = None
    """
    Git tag.
    """
    branch: StrictStr | None = None
    """
    Git branch.
    """
    model_config = default_pydantic_options()

    @model_validator(mode="before")
    @classmethod
    def _check_mutually_exclusive_fields(cls, values: Any) -> Any:
        if not isinstance(values, dict):
            return values
        keys_in_dict = sum(1 for key in ("commit", "tag", "branch") if key in values)
        if keys_in_dict > 1:
            raise ValueError("Expected at most one of 'commit', 'tag', 'branch'")
        return values  # pyright: ignore [reportUnknownVariableType]

    @field_serializer("git_url")
    def _serialize_git(self, url: AnyUrl) -> str:
        return str(url)
