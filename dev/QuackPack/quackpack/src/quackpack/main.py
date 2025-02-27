import logging
import os
from typing import Final, Never

from quackpack.cli import action_for, get_parser
from quackpack.errors import QuackPackError
from quackpack.global_info import GlobalInfo
from quackpack.logger import get_logger


def logger() -> logging.Logger:
    """
    Get `logging.Logger` for this module.
    """
    return get_logger(__name__)


def real_main(info: GlobalInfo) -> int:
    """
    Real entry point for Quack Pack main.
    ----
    Args:
    - `info`: `GlobalInfo` created in `__init__.py`.
    ----
    Returns:
    - `int`: Exit code.
    """
    expand_user_aliases(info)
    logger().debug(f"Real main with {info}")
    # Extra args are for foreign subcommands.
    known_args, extra_args = get_parser().parse_known_args(info.qp_args)
    command = known_args.command
    if command is not None and info.script_args is not None:
        raise QuackPackError("Simultaneous script execution with positional arguments is not supported")
    if info.script_args is not None:
        execute_script(info.script_args)
        raise QuackPackError("Script execution failed")
    if command is None:
        raise QuackPackError("Missing positional command")

    action = action_for(command)
    # We are executing external subcommand.
    if action is None:
        handle_external_command(command, extra_args)
    if len(extra_args) > 0:
        raise QuackPackError(f"Unrecognised cli arguments {extra_args}")
    info.parsed_args = known_args
    return action(info)


def expand_user_aliases(info: GlobalInfo) -> None:
    """
    Expand user aliases.
    ----
    Args:
    - `info`: `GlobalInfo` with user configuration and cli arguments.
    """
    for idx, arg in enumerate(info.qp_args):
        # Skip optional arguments.
        if arg.startswith("-"):
            continue
        # First positional argument is not an alias.
        if arg not in info.user_config.aliases:
            return
        logger().debug(f"Expanding user alias '{arg}' to '{info.user_config.aliases[arg]}'")
        if isinstance(info.user_config.aliases[arg], str):
            # We just checked above, that it's str.
            alias: str = info.user_config.aliases[arg]  # pyright: ignore[reportAssignmentType]
            to_replace = alias.split()
        else:
            # FIXME: Pyright doesn't recognise, that in branches we have `str` or `list[str]`, and tries to suggest only common methods.
            assert isinstance(info.user_config.aliases[arg], list), "Pydantic should prevent that"
            to_replace: list[str] = info.user_config.aliases[arg]  # pyright: ignore[reportAssignmentType]
        info.qp_args[idx : idx + 1] = to_replace
        return expand_user_aliases(info)


def execute_script(args: list[str]) -> Never:
    """
    Execute Duckling script from `args`.
    ----
    Args:
    - `args`: list with script name and script arguments.
    """
    script_name = args[0]
    script_args = args[1:]
    logger().debug(f"Executing script '{script_name}' with arguments {script_args}")
    logger().debug("Implement script execution")
    raise NotImplementedError


def handle_external_command(name: str, args: list[str]) -> Never:
    """
    Execute external subcommand.
    ----
    Args:
    - `name`: Name of the subcommand.
    - `args`: Arguments for the subcommand.
    """
    PREFIX: Final[str] = "qp-"
    full_name = PREFIX + name
    # argv[0] always should be executable name.
    args.insert(0, full_name)
    logger().debug(f"Executing external command '{name}' with arguments {args}")
    os.execvp(full_name, args)
    raise QuackPackError("Exec failed")
