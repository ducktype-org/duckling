import argparse
from dataclasses import dataclass

from quackpack.config.user import Config
from quackpack.console import Console


@dataclass
class GlobalInfo:
    """
    Global informations used everywhere throughout Quack Pack implementation.
    """

    user_config: Config
    """
    User configuration.
    """
    qp_args: list[str]
    """
    Quack Pack specific arguments, which can be later passed to main parser.
    """
    script_args: list[str] | None
    """
    Arguments needed for running scripts.

    Note that if `script_args` are not `None`, then `qp_args`'s subcommand should be `None`.
    """
    console: Console
    """
    `quackpack.console.Console` used for printing to user.
    """
    parsed_args: argparse.Namespace | None = None
