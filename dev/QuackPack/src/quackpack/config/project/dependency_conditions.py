from pydantic import BaseModel, StrictStr

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.pkgid import Identifier


# TODO: add discriminators for all of the unions
class DependencyConditions(BaseModel):
    """
    Optional conditions of the dependency.
    """

    # FIXME: Should it be `Identifier | list[Identifier] | None`?
    system: StrictStr | list[StrictStr] | None = None
    """
    If not `None`, then match host system against provided system(s).
    """

    # FIXME: Should it be `Identifier | list[Identifier] | None`?
    arch: StrictStr | list[StrictStr] | None = None
    """
    If not `None`, then match host CPU architecture against provided architecture(s).
    """

    project_flags: Identifier | list[Identifier] | None = None  # TODO: Convert type to logic string.
    """
    If not `None`, then enable this dependency only if we are building project with any of the `project_flags`.
    """

    model_config = DEFAULT_MODEL_CONFIG
