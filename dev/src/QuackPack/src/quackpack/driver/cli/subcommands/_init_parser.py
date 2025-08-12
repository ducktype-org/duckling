from pathlib import Path

from quackpack.driver.commands.init import InitOptions, NewVenvType, create_at
from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``init`` subcommand.

    :return: The parser configured for the ``init`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    # FIXME: Package templates.
    return (
        CliParser.subcommand(name="init", description="Initialize a new package")
        .add_path(long_name="path", help="Path to the new package", argument_count=ArgumentCount.Optional)
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--venv", help="Initialize venv instead of a package")
            .add_flag(long_name="--full", help="Initialize full package, with prompts")
        )
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--ephemeral", help="Mark new venv as ephemeral")
            .add_flag(long_name="--local-storage", help="Use local package storage instead of shared storage")
        )
        .add_flag(
            long_name="--expose-freezefile",
            help="Makes synchronization export a freezefile and use the provided one",
        )
        .add_str(long_name="--name", help="Override package name")
    )


def execute(ctx: GlobalContext, args: Arguments) -> None:
    """
    Execute the ``init`` subcommand to initialize a new package or virtual environment.

    :param quackpack.util.global_context.GlobalContext ctx: The global context containing configuration and state.
    :param quackpack.cli._arguments.Arguments args: Parsed command-line arguments for this subcommand.
    """

    venv_path = args.matched.path.expanduser().resolve() if args.matched.path is not None else Path.cwd()
    venv_name = args.matched.name if args.matched.name is not None else venv_path.name
    if args.matched.venv:
        venv_type = NewVenvType.PlainVenv
    elif args.matched.full:
        venv_type = NewVenvType.Full
    else:
        venv_type = NewVenvType.Binary

    is_ephemeral: bool = args.matched.ephemeral
    use_local_storage: bool = args.matched.local_storage
    expose_freezefile: bool = args.matched.expose_freezefile

    options = InitOptions(
        destination=venv_path,
        name=venv_name,
        type=venv_type,
        is_ephemeral=is_ephemeral,
        use_local_storage=use_local_storage,
        expose_freezefile=expose_freezefile,
        ctx=ctx,
    )
    create_at(options)
