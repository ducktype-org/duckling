from quackpack.util.global_context import GlobalContext

from ._parser import CliParser, ExclusiveGroup


def get_parser() -> CliParser:
    """
    Get the argument parser for `test` command.
    ----
    Returns:
    - `CliParser`: Parser for `test` command.
    """
    return (
        CliParser.subcommand(name="test", description="Test a current project")
        .add_jobs()
        .add_exclusive_group(group=ExclusiveGroup().add_profile().add_release())
        .add_exclusive_group(
            group=ExclusiveGroup()
            .add_flag(long_name="--locked", help="Assume freezefile doesn't change")
            .add_flag(long_name="--frozen", help="Don't update freezefile")
            .add_flag(long_name="--offline", help="Don't perform network requests")
        )
        .add_duckc()
    )


def execute(ctx: GlobalContext) -> None:
    """
    Execute this subcommand.
    ----
    Args:
    - `ctx`: all possibly needed context for this function.
    """
    ctx.console.debug("Implement 'execute()' for 'test'")
    raise NotImplementedError
