import logging
from os import getenv
from typing import Final

from rich.logging import RichHandler

# NOTE: Logger name should correspond to full file module-like path.
# All attributes are here: https://docs.python.org/3/library/logging.html#logrecord-attributes.
__FORMAT: Final[str] = "%(name)s:%(lineno)s %(message)s"
__DATE_FORMAT: Final[str] = "[%X.%f]"
_debug_env_value: str | None = None


class QuackPackDebugFilter(logging.Filter):
    def __init__(self, name: str = ""):
        self._cached_filter: bool | None = None
        super().__init__(name)

    def filter(self, record: logging.LogRecord) -> bool | logging.LogRecord:
        # NOTE: This works, because filtering records is only based on logger's name,
        #       and each logger has its own instance of QuackPackDebugFilter.
        # PERF: Cache results, so string operations don't become bottle-neck.
        if self._cached_filter is None:
            filename = record.name
            module_name = filename.rsplit(".", maxsplit=1)[0]
            # NOTE: This is for command-line debugging particular modules, like in cargo: https://doc.crates.io/contrib/implementation/debugging.html.
            self._cached_filter = _debug_env_value in (filename, module_name, "quackpack", "qp", "all")
        return self._cached_filter


def setup_logger() -> None:
    """
    Setup logger for debug printing.
    """
    # PERF: Cache environmental variable, so getenv() doesn't become bottle-neck.
    global _debug_env_value
    _debug_env_value = getenv("QP_DEBUG")
    # NOTE: We set every logger level to DEBUG, and instead rely on filters.
    logging.basicConfig(
        level=logging.DEBUG,
        format=__FORMAT,
        datefmt=__DATE_FORMAT,
        force=True,
        handlers=[
            RichHandler(
                rich_tracebacks=True, show_time=True, markup=True, omit_repeated_times=False, show_path=False
            )
        ],
    )


def get_logger(name: str) -> logging.Logger:
    """
    Get logger with name `name`.
    Because Python does not have lazy initialization of global variables, there should not be a global `logger = get_logger(__name__)` variable; always wrap it in a function or create local variable when needed.
    `__name__` should always be passed as a `name` (but for Python reasons it can't be set here as a default, because it'd use this file `__name__`, therefor caller should pass its own).
    Loggers should only be used with `.debug()` calls.
    ----
    Args:
    - `name`: name of the logger.
    ----
    Returns:
    - `logging.Logger`: logger for provided `name`.
    """
    logger = logging.getLogger(name)
    logger.addFilter(QuackPackDebugFilter())
    return logger
