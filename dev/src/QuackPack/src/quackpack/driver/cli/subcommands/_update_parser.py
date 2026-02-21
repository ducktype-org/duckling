"""
TODO
Note that this subcommand is currently not included in argparser.

It became clear that the meaning of `update` is not easy to decide for.

Firstly, the command should not update to incompatible version without user interaction;
breaking our dependencies is not nice.

If it were to be limited to updating to compatible versions however, it would not be very useful
as the solver can bump declared version to any newer compatible one, so `update` is mostly
not needed there - outside of updating for security patch (about which the info should
probably be handled with some audit system), bumping the dependency version in the manifest
only hurts as minimal working version can lead to better solver solutions and is more clear
regarding to APIs required from the dependency.

It would also make sense for user to use `update` command without a priori knowing,
what precise updates are there to perform, in which case the command should somehow
inform the user about possible updates.
Then maybe in seperate execution (or even interactive session?), after checking what changes
were made in the new versions the user could choose what updates to perform.

Also some things to keep in mind:
- used features should be checked if they still exist in the new version
"""

from quackpack.util.global_context import GlobalContext

from ._arguments import Arguments
from ._parser import ArgumentCount, CliParser


def get_parser() -> CliParser:
    """
    Get the CLI argument parser for the ``update`` command.

    :return: The parser configured for the ``update`` command.
    :rtype: quackpack.cli._parser.CliParser
    """

    return CliParser.subcommand(
        name="update", description="Update packages in the current venv"
    ).add_str(
        long_name="packages",
        help="Packages to update",
        argument_count=ArgumentCount.OneOrMore,
    )


def execute(ctx: GlobalContext, _args: Arguments) -> None:
    """
    Execute the ``update`` subcommand to update packages in the current venv.

    :param quackpack.util.global_context.GlobalContext ctx: Context providing necessary runtime information.
    :param quackpack.cli._arguments.Arguments _args: Parsed arguments from the CLI for this command.
    :raises NotImplementedError: Function is not yet implemented.
    """

    ctx.console.debug("Implement 'execute()' for 'update'")
    raise NotImplementedError
