import os
from contextlib import chdir
from typing import Never

from quackpack.cli import EXTERNAL_CMD_PREFIX, action_for, get_parser
from quackpack.util.errors import QuackPackError
from quackpack.util.global_context import GlobalContext
from quackpack.util.levenshtein import distance
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


def run(ctx: GlobalContext) -> None:
    """
    Real entry point for Quack Pack main.
    ----
    Args:
    - `ctx`: `GlobalContext` created in `__init__.py`.
    """

    parser = get_parser()
    fix_user_typos(ctx, parser.subcommands)
    expand_user_aliases(ctx)
    # Extra args are for foreign subcommands.
    known_args, extra_args = parser.parse_known_args(ctx.qp_args)
    if known_args.quiet:
        ctx.console.quiet = True
        ctx.error_console.quiet = True
    elif known_args.verbose:
        ctx.console.set_verbose()
        ctx.error_console.set_verbose()

    command = known_args.command
    if command is not None and ctx.script_args is not None:
        raise QuackPackError("Simultaneous script execution with positional arguments is not supported")
    if ctx.script_args is not None:
        execute_script(ctx.script_args)
        raise QuackPackError("Script execution failed")
    if command is None:
        raise QuackPackError("Missing positional command")

    action = action_for(command)
    # We are executing external subcommand.
    if action is None:
        handle_external_command(command, extra_args)
    if extra_args:
        raise QuackPackError(f"Unrecognised cli arguments {extra_args}")
    ctx.parsed_args = known_args
    logger.debug(f"Calling action with {ctx}")
    logger.debug(f"Changing CWD to '{known_args.directory}'")
    with chdir(known_args.directory):
        action(ctx)


def fix_user_typos(ctx: GlobalContext, cli_commands: list[str]) -> None:
    """
    Try to fix first positional command with Levenshtein distance.

    We're getting all possible targets from cli, then turn them into tuples (target, distance),
    next we filter by distance, and check, if we have 0, 1, or more matches, and take appropriate action.
    ----
    Args:
    - `ctx`: `GlobalContext`.
    - `cli_commands`: list of main cli subcommands.
    """
    if not ctx.configuration.security.typo_tolerance.enabled:
        return
    max_distance = ctx.configuration.security.typo_tolerance.max_distance
    targets: set[str] = set()
    targets.update(*cli_commands)
    logger.debug(f"Matching Levenshtein with {max_distance=} against {targets}")
    for idx, arg in enumerate(ctx.qp_args):
        if arg.startswith("-"):
            continue
        match_against = arg
        match_idx = idx
        break
    else:
        return
    matched = [x for x in targets if distance(match_against, x) <= max_distance]
    if not matched:
        return
    if len(matched) > 1:
        formatted = ", ".join(f"'{x}'" for x in matched)
        raise QuackPackError(f"Ambigous expansion of '{match_against}'. Can choose from {formatted}.")
    logger.debug(f"Levenshtein changed '{match_against}' to '{matched[0]}'")
    ctx.qp_args[match_idx] = matched[0]


def expand_user_aliases(ctx: GlobalContext) -> None:
    """
    Expand user aliases.
    ----
    Args:
    - `ctx`: `GlobalContext` with user configuration and cli arguments.
    """
    for idx, arg in enumerate(ctx.qp_args):
        # Skip optional arguments.
        if arg.startswith("-"):
            continue
        # First positional argument is not an alias.
        if arg not in ctx.configuration.aliases:
            return
        logger.debug(f"Expanding user alias '{arg}' to '{ctx.configuration.aliases[arg]}'")
        if isinstance(ctx.configuration.aliases[arg], str):
            # We just checked above, that it's str.
            alias: str = ctx.configuration.aliases[arg]  # pyright: ignore[reportAssignmentType]
            to_replace = alias.split()
        else:
            # FIXME: Pyright doesn't recognise, that in branches we have `str` or `list[str]`, and tries to suggest only common methods.
            assert isinstance(ctx.configuration.aliases[arg], list), "Pydantic should prevent that"
            to_replace: list[str] = ctx.configuration.aliases[arg]  # pyright: ignore[reportAssignmentType]
        ctx.qp_args[idx : idx + 1] = to_replace
        return expand_user_aliases(ctx)


# FIXME: This should live in its own file.
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


def handle_external_command(name: str, args: list[str]) -> Never:
    """
    Execute external subcommand.
    ----
    Args:
    - `name`: Name of the subcommand.
    - `args`: Arguments for the subcommand.
    """
    full_name = EXTERNAL_CMD_PREFIX + name
    # argv[0] always should be executable name.
    args.insert(0, full_name)
    logger.debug(f"Executing external command '{name}' with arguments {args}")
    os.execvp(full_name, args)
    raise QuackPackError("Exec failed")
