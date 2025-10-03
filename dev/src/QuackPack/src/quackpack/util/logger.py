import logging
from typing import Final, override

from rich.logging import RichHandler

# NOTE: Logger name should correspond to full file module-like path.
# All attributes are here: https://docs.python.org/3/library/logging.html#logrecord-attributes.
__FORMAT: Final[str] = "%(name)s:%(lineno)s %(message)s"
__DATE_FORMAT: Final[str] = "[%X.%f]"
_debug_env_value: str | None = None

__all__ = ["get_logger", "setup_logger"]


class QuackPackDebugFilter(logging.Filter):
    """
    Filter for loggers using `"QP_DEBUG"` environmental variable.
    """

    def __init__(self, name: str = ""):
        self._cached_filter: bool | None = None
        super().__init__(name)

    @override
    def filter(self, record: logging.LogRecord) -> bool | logging.LogRecord:
        # NOTE: This works, because filtering records is only based on logger's name,
        #       and each logger has its own instance of QuackPackDebugFilter.
        # PERF: Cache results, so string operations don't become bottleneck.
        if self._cached_filter is None:
            filename = record.name
            module_name = filename.rsplit(".", maxsplit=1)[0]
            # NOTE: This is for command-line debugging particular modules, like in cargo: https://doc.crates.io/contrib/implementation/debugging.html.
            is_good_module = module_name.startswith(("quackpack", "qp"))
            self._cached_filter = is_good_module and (
                _debug_env_value in (filename, module_name, "quackpack", "qp", "all")
                or (
                    _debug_env_value is not None
                    and module_name.startswith(_debug_env_value)
                )
            )
        return self._cached_filter


def setup_logger(debug_env_value: str | None) -> None:
    """
    Setup logger for debug printing.
    """
    # PERF: Cache environmental variable, so getenv() doesn't become bottle-neck.
    global _debug_env_value
    _debug_env_value = debug_env_value
    # NOTE: We don't set default log level here (in a root logger), because that affects loggers of dependencies (looking at you, gitpython).
    logging.basicConfig(
        format=__FORMAT,
        datefmt=__DATE_FORMAT,
        force=True,
        handlers=[
            RichHandler(
                rich_tracebacks=True,
                show_time=True,
                markup=True,
                omit_repeated_times=False,
                show_path=False,
            )
        ],
    )


def get_logger(name: str) -> logging.Logger:
    """
    Get logger with name `name`.

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
    # For our loggers, we are setting DEBUG level and rely on filter.
    logger.addFilter(QuackPackDebugFilter())
    logger.setLevel(logging.DEBUG)
    return logger
