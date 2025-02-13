import logging
from argparse import Namespace
from os import getenv
from typing import Final

from rich.logging import RichHandler

__FORMAT: Final[str] = "%(message)s"
__DATE_FORMAT: Final[str] = "[%x %X.%f]"


# TODO: Do we want to change anything more than loglevel? (Idk, if time should be printed, formats, etc)
#       (This of course requires changes in main parser.)
def setup_logger() -> None:
    """
    Setups logger for debug printing.
    """
    logging.basicConfig(
        level=logging.INFO,
        format=__FORMAT,
        datefmt=__DATE_FORMAT,
        handlers=[
            RichHandler(
                rich_tracebacks=True, show_time=False, markup=True, omit_repeated_times=False, show_path=False
            )
        ],
    )


# FIXME: always passing `__name__` on the call site can be awkward. We might not want to do this (so `get_logger()` is equivalent to `get_logger(__name__)`), but:
#        1. if we set default argument here as `__name__`, it'll be this file `__name__`, not the caller (unlike in C++),
#        2. we CAN get `__name__` of the caller, but it's hacky, requires looking at the call stack, and this sounds like something, that could brake: https://stackoverflow.com/a/1095621.
def get_logger(name: str) -> logging.Logger:
    """
    Get logger with name `name`.
    It should be used like this: `logger = get_logger(__name__)`, at the top of the file.
    ----
    Args:
    - `name`: name of the logger.
    ----
    Returns:
    - `logging.Logger`: logger for provided `name`.
    """
    logger = logging.getLogger(name)
    # NOTE: This is for command-line debugging particular modules, like in cargo: https://doc.crates.io/contrib/implementation/debugging.html.
    module_name = name.rsplit(".", maxsplit=1)[0]
    if getenv("QP_DEBUG") in (module_name, "quackpack"):
        logger.setLevel(logging.DEBUG)
    return logger
