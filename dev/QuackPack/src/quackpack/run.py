import sys
from contextlib import chdir, nullcontext
from pathlib import Path
from typing import cast

from quackpack.cli import Arguments, action_for, builtin_aliases, get_parser, handle_external_command
from quackpack.scripts import execute_script, split_args_for_script
from quackpack.util.errors import QuackPackError
from quackpack.util.global_context import GlobalContext
from quackpack.util.levenshtein import distance
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


def run(ctx: GlobalContext) -> int | None:
    """
    Real entry point for Quack Pack main.
    ----
    Args:
    - `ctx`: `GlobalContext` created in `__init__.py`.
    """

    # NOTE: We need to remove executable name (sys.argv[0]) from arguments, because of how parser works...
    qp_args, script_args = split_args_for_script(sys.argv[1:])
    logger.debug(f"Quack Pack arguments are {qp_args}, extra script arguments are {script_args}")
    # We execute script, but arguments are missing.
    if script_args is not None and not script_args:
        ctx.error_console.warn("Empty script arguments are not supported")
        return 1

    parser = get_parser()
    # FIXME: Perform some clever checks, that we don't have cycle.
    cli_commands = parser.subcommands.copy()
    cli_commands += list(ctx.configuration.aliases.keys())
    for x in builtin_aliases().values():
        cli_commands += x
    fix_user_typos(ctx, qp_args, cli_commands)
    # NOTE: We use `list` to preserve the order.
    # FIXME: Something like OrderedSet would be better.
    already_expanded_aliases: list[str] = []
    expand_user_aliases(ctx, qp_args, already_expanded_aliases)
    # Extra args are for foreign subcommands.
    known_args, extra_args = parser.parse_known_args(qp_args)
    if known_args.quiet:
        ctx.console.quiet = True
        ctx.error_console.quiet = True
    elif known_args.verbose:
        ctx.console.set_verbose()
        ctx.error_console.set_verbose()

    # FIXME: We can't set `color_system` after constructing, so, theoretically, we ignore `--color=auto`.
    #
    #        I see one workaround: in `setup_and_run` we create like really early console (only to catch parser exceptions),
    #        then we create `ctx.console` and `ctx.error_console` here, providing `color_system`. But this approach has one downside:
    #        error handling is done both in `setup_and_run` and `run`, and here we'd have big `try-except` block.
    if known_args.color == "never":
        ctx.console.no_color = True
        ctx.error_console.no_color = True
    elif known_args.color == "always":
        ctx.console.no_color = False
        ctx.error_console.no_color = False

    command = known_args.command
    if command is not None and script_args is not None:
        raise QuackPackError("Simultaneous script execution with positional arguments is not supported")
    if script_args is not None:
        execute_script(script_args)
        raise QuackPackError("Script execution failed")
    if command is None:
        raise QuackPackError("Missing positional command")

    action = action_for(command)
    # We are executing external subcommand.
    if action is None:
        handle_external_command(command, extra_args)
    if extra_args:
        raise QuackPackError(f"Unrecognised cli arguments {extra_args}")
    chdir_target = known_args.directory or Path.cwd()
    should_chdir = chdir_target != Path.cwd()
    if should_chdir:
        logger.debug(f"Changing CWD to '{chdir_target}'")
    args = Arguments(matched=known_args)
    with chdir(chdir_target) if should_chdir else nullcontext():
        logger.debug(f"Calling action with {ctx=} and {args=}")
        action(ctx, args)


# FIXME: Idealnie zamiast na `list[str]`, operowalibyśmy na `argparse.Namespace` i wywoływali parser, żeby doparsować matche z aliasów.
# Tylko wtedy trzeba ręcznie handlować `--help` i bawić się z unknown_commands.
# Chociaż i tak w Pythonie się nie da, bo argparse jest ułomny.
def fix_user_typos(ctx: GlobalContext, qp_args: list[str], cli_commands: list[str]) -> None:
    """
    Try to fix first positional command with Levenshtein distance.

    We're getting all possible targets from cli, then turn them into tuples (target, distance),
    next we filter by distance, and check, if we have 0, 1, or more matches, and take appropriate action.
    ----
    Args:
    - `ctx`: `GlobalContext`.
    - `cli_commands`: list of main cli subcommands.
    """
    should_error_on_any_match = not ctx.configuration.security.typo_tolerance.enabled
    max_distance = ctx.configuration.security.typo_tolerance.max_distance
    targets: set[str] = set()
    targets.update(cli_commands)
    logger.debug(f"Matching Levenshtein with {max_distance=} against {targets}")
    should_skip_next_arg: bool = False
    for idx, arg in enumerate(qp_args):
        if should_skip_next_arg:
            should_skip_next_arg = False
            continue
        if _ugly_check_should_skip_next_arg(arg):
            should_skip_next_arg = True
            continue
        if arg.startswith("-"):
            continue
        logger.debug(f"Trying Levenshtein against {arg}")
        match_against = arg
        match_idx = idx
        break
    else:
        return
    if match_against in targets:
        return
    matched = [x for x in targets if distance(match_against, x) <= max_distance]
    if not matched:
        return
    if should_error_on_any_match or len(matched) > 1:
        formatted = "".join(f"   '{x}'\n" for x in matched)
        raise QuackPackError(f"""No such command as '{match_against}'. Did you mean:

{formatted}?""")
    logger.debug(f"Levenshtein changed '{match_against}' to '{matched[0]}'")
    qp_args[match_idx] = matched[0]


def expand_user_aliases(ctx: GlobalContext, qp_args: list[str], already_expanded_aliases: list[str]) -> None:
    """
    Expand user aliases.
    ----
    Args:
    - `ctx`: `GlobalContext` with user configuration and cli arguments.
    """
    should_skip_next_arg: bool = False
    for idx, arg in enumerate(qp_args):
        if should_skip_next_arg:
            should_skip_next_arg = False
            continue
        if _ugly_check_should_skip_next_arg(arg):
            should_skip_next_arg = True
            continue
        # Skip optional arguments.
        if arg.startswith("-"):
            continue
        # First positional argument is not an alias.
        logger.debug(f"Trying alias against {arg}")
        if arg not in ctx.configuration.aliases:
            return
        if arg in already_expanded_aliases:
            # Ensure current alias appears in trace.
            already_expanded_aliases.append(arg)
            backtrace = " -> ".join(already_expanded_aliases)
            raise QuackPackError(f"Alias {arg} was already expanded. Found cycle: '{backtrace}'")
        already_expanded_aliases.append(arg)
        logger.debug(f"Expanding user alias '{arg}' to '{ctx.configuration.aliases[arg]}'")
        if isinstance(ctx.configuration.aliases[arg], str):
            to_replace = cast(str, ctx.configuration.aliases[arg]).split()
        else:
            to_replace = cast(list[str], ctx.configuration.aliases[arg])
        qp_args[idx : idx + 1] = to_replace
        return expand_user_aliases(ctx, qp_args, already_expanded_aliases)


# FIXME: This is ugly hack remove this.
# Problem: Levenshtein i aliasy patrzą na pierwszy argument bez `-` na początku, a powinny patrzyć na pierwszą komendę (argument traktowany jako komenda).
#
# Dlaczego: jeśli odpalimy `qp -C dir build`, to levenshtein i aliasy będą targetować `dir`, a nie `build`.
#
# Ładne rozwiązanie: Levenshtein i aliasy powinny operować na jakimś typie od parsera i wywoływać parser rekurencyjnie.
#
# Dlaczego ładne rozwiązanie nie działa (w pythonie): bo argparse rzuca wyjątek, gdy mu się coś nie spodoba.
#
# (Mogę wysłać prototyp, jak to zrobić w ruście).
#
# Ale to i tak nie zawsze działa xD, np `qp -C --color --color=always ...`, bo argparse twierdzi, że `--color` nie idzie do `-C`...
def _ugly_check_should_skip_next_arg(current: str) -> bool:
    return current in {"--directory", "-C", "--color"}
