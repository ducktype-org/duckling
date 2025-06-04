from typing import Never

from quackpack.util.logger import get_logger

logger = get_logger(__name__)


def split_args_for_script(arguments: list[str]) -> tuple[list[str], list[str] | None]:
    """
    Split `arguments` into a program and a script arguments.
    ----
    Args:
    - `arguments`: list of arguments to be split at first double dash.
    ----
    Returns:
    - `tuple[list[str], list[str] | None]` - `tuple` with `list` of program arguments and optional script arguments.
    """
    # We split cli arguments at first double dash, to distinguish between Quack Pack arguments,
    # and extra script arguments.
    double_dash_idx = [*arguments, "--"].index("--")
    return (
        (arguments, None)
        if double_dash_idx == len(arguments)
        else (arguments[:double_dash_idx], arguments[double_dash_idx + 1 :])
    )


def execute_script(args: list[str]) -> Never:
    """
    Execute Duckling script from `args`.
    ----
    Args:
    - `args`: list with script name and script arguments.
    """
    script_name = args[0]
    script_args = args[1:]
    logger.debug(f"Executing script '{script_name}' with arguments {script_args}")
    logger.debug("Implement script execution")
    raise NotImplementedError
