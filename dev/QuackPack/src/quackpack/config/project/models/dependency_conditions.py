from pydantic import BaseModel, StrictStr

from quackpack.util.default_pydantic_options import default_pydantic_options


# TODO: add discriminators for all of the unions
class DependencyConditions(BaseModel):
    """
    Optional conditions of the dependency.
    """

    system: StrictStr | list[StrictStr] | None = None
    """
    If not `None`, then match host system against provided system(s).
    """
    arch: StrictStr | list[StrictStr] | None = None
    """
    If not `None`, then match host CPU architecture against provided architecture(s).
    """
    project_flags: StrictStr | list[StrictStr] | None = None  # TODO: Convert type to logic string.
    """
    If not `None`, then enable this dependency only if we are building project with any of the `project_flags`.
    """

    model_config = default_pydantic_options()
