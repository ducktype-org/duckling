from quackpack.commands.init import InitOptions, NewVenvType, create_at
from quackpack.util.global_context import GlobalContext

from ._parser import CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the argument parser for `init` command.
    ----
    Returns:
    - `CliParser`: Parser for `init` command.
    """
    # FIXME: Project templates.
    return (
        CliParser.subcommand(name="init", description="Initialize a new project")
        .add_path(long_name="path", help="Path to the new project")
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--venv", help="Initialize venv instead of a project")
            .add_flag(long_name="--full", help="Initilize full project, with prompts")
        )
        .add_str(long_name="--name", help="Override project name")
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    venv_path = ctx.parsed_args.path.expanduser()
    venv_name = ctx.parsed_args.name or venv_path.name
    if ctx.parsed_args.venv:
        venv_type = NewVenvType.PlainVenv
    elif ctx.parsed_args.full:
        venv_type = NewVenvType.Full
    else:
        venv_type = NewVenvType.Binary
    options = InitOptions(destination=venv_path, name=venv_name, type=venv_type, ctx=ctx)
    create_at(options)
