from ._parser import CliParser, ExclusiveGroup


def get_early_parser() -> CliParser:
    """
    Get the early-stage main CLI argument parser for Quack Pack, without any subcommands.

    :return: The main parser configured with global options.
    :rtype: quackpack.cli._parser.CliParser
    """

    return (
        CliParser.main()
        .add_version()
        .add_chdir()
        .add_exclusive_group(group=ExclusiveGroup().add_quiet().add_verbose())
        .add_colors()
    )
