import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import Final

from quackpack.config.project import Venv
from quackpack.config.user import Config
from quackpack.util.console import Console

__all__ = ["DEFAULT_DUCKC_BINARY", "GlobalContext"]

DEFAULT_DUCKC_BINARY: Final[str] = "duckc"
DEFAULT_BUILD_PROFILE: Final[str] = "debug"


@dataclass(frozen=False, kw_only=True)
class GlobalContext:
    """
    Global information used everywhere throughout Quack Pack implementation.
    """

    configuration: Config
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
    `quackpack.util.console.Console` used for printing to user.
    """
    error_console: Console
    """
    `quackpack.util.console.Console` set up for `sys.stderr`.
    """
    _parsed_args: argparse.Namespace | None = None
    """
    Fully parsed user arguments.
    """
    frozen: bool = False
    """
    If true, then don't update freezefile.
    """
    locked: bool = False
    """
    If true, then error if freezefile would've changed.
    """
    offline: bool = False
    """
    If true, then don't perform any network requests.
    """
    duckc_binary: str = DEFAULT_DUCKC_BINARY
    """
    Name of the Duckling compiler.
    """
    build_profile: str = DEFAULT_BUILD_PROFILE
    """
    Current build profile.
    """
    _venv: Venv | None = None
    """
    Optional current venv.
    """

    # NOTE: If we set default in the function arguments, it won't be affected by `chdir()`.
    def reload_venv(self, cwd: Path | None = None) -> None:
        self._venv = Venv.find_from(cwd or Path.cwd())

    @property
    def venv(self) -> Venv:
        """
        Current venv.
        """
        assert self._venv is not None, "venv without reload_venv()"
        return self._venv

    @property
    def parsed_args(self) -> argparse.Namespace:
        """
        Fully parsed user arguments.
        """
        assert self._parsed_args is not None, "parsed_args get without set"
        return self._parsed_args

    @parsed_args.setter
    def parsed_args(self, args: argparse.Namespace) -> None:
        self._parsed_args = args
