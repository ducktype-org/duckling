from quackpack.commands.init import InitOptions, NewVenvType, create_at
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``init`` subcommand.

    :return: The parser configured for the ``init`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    # FIXME: Project templates.
    return (
        CliParser.subcommand(name="init", description="Initialize a new project")
        .add_path(long_name="path", help="Path to the new project")
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--venv", help="Initialize venv instead of a project")
            .add_flag(long_name="--full", help="Initialize full project, with prompts")
        )
        .add_str(long_name="--name", help="Override project name")
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``init`` subcommand to initialize a new project or virtual environment.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments args: Parsed command-line arguments for this subcommand.
    """

    venv_path = args.matched.path.expanduser()
    venv_name = args.matched.name or venv_path.name
    if args.matched.venv:
        venv_type = NewVenvType.PlainVenv
    elif args.matched.full:
        venv_type = NewVenvType.Full
    else:
        venv_type = NewVenvType.Binary
    options = InitOptions(destination=venv_path, name=venv_name, type=venv_type, ctx=ctx)
    create_at(options)
