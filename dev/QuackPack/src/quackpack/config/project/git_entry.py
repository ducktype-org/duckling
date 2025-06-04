from typing import Self, override

from pydantic import AnyUrl, BaseModel, StrictStr, field_serializer, model_validator

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG


# TODO: What actually we need?
class GitEntry(BaseModel):
    """
    Git source of the dependency.
    """

    git_url: AnyUrl
    """
    Git url.
    """

    # FIXME: Validate, that commit is a valid git commit.
    #        We can do `len(commit) <= 40` and that all characters
    #        are lower-key hex ASCII?
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

    model_config = DEFAULT_MODEL_CONFIG

    @override
    def __hash__(self) -> int:
        return hash((self.git_url, self.commit, self.tag, self.branch))

    @model_validator(mode="after")
    def _check_mutually_exclusive_fields(self) -> Self:
        number_of_not_nones = (self.tag is not None) + (self.branch is not None)
        if number_of_not_nones > 1:
            raise ValueError("Expected at most one of 'tag' or 'branch'")
        return self

    @field_serializer("git_url")
    def _serialize_git(self, url: AnyUrl) -> str:
        return str(url)
