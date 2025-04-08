from ._parser import CliParser, ExclusiveGroup


def get_early_parser() -> CliParser:
    """
    Get early (unfinished) main Quack Pack argument parser.
    ----
    Returns:
    - `CliParser`: Main Quack Pack parser, but without any subcommands.
    """
    return (
        CliParser.main()
        .add_version()
        .add_chdir()
        .add_exclusive_group(group=ExclusiveGroup().add_quiet().add_verbose())
    )
