from dataclasses import dataclass
from typing import Final

from quackpack.config.user import Configuration
from quackpack.util.console import Console

__all__ = ["DEFAULT_BUILD_PROFILE", "DEFAULT_DUCKC_BINARY", "GlobalContext"]


DEFAULT_DUCKC_BINARY: Final[str] = "duckc"
DEFAULT_BUILD_PROFILE: Final[str] = "debug"


@dataclass(frozen=False, kw_only=True)
class GlobalContext:
    """
    Global information used everywhere throughout Quack Pack implementation.
    """

    configuration: Configuration
    """
    User configuration.
    """

    console: Console
    """
    `quackpack.util.console.Console` used for printing to user.
    """

    error_console: Console
    """
    `quackpack.util.console.Console` set up for `sys.stderr`.
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
