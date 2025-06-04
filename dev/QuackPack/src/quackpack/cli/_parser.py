from __future__ import annotations

import argparse
from collections import defaultdict
from collections.abc import Sequence
from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path
from typing import Any, Final, Self, cast

from rich_argparse import RichHelpFormatter

from quackpack.util.cpu import get_logical_threads
from quackpack.util.global_context import DEFAULT_BUILD_PROFILE, DEFAULT_DUCKC_BINARY

ARGS_STYLE: Final[str] = "bold cyan"


# These options are supposed to go as `str(...)` for `nargs=` option.
# Python documentation allows one more special value to be passed as `nargs`,
# which we DON'T use here: it is an integer N. Reason is, that passing
# `nargs=N` produces list of `N` elements, which is counter-intuitive
# in `nargs=1` scenario, as we'd get one element list.
#
# Since we (currently) don't need this feature, let's not use it, and keep code simpler.
# Also this follows, that passing `argument_count=None` (default)
# to `.add_str()` (right now only method that has multiple values)
# yields expected behaviour of requiring exactly one argument.
#
# The only exception from rule above is that `default=<not None>, argument_count=None`
# is equivalent to `default=<not None>, argument_count=ArgumentCount.Optional`.
class ArgumentCount(StrEnum):
    OneOrMore = argparse.ONE_OR_MORE
    ZeroOrMore = argparse.ZERO_OR_MORE
    Optional = argparse.OPTIONAL


class CliParser(argparse.ArgumentParser):
    """
    CLI argument parser with rich formatting and helper methods for common options.
    """

    @classmethod
    def setup_formatter(cls) -> None:
        RichHelpFormatter.styles["argparse.args"] = ARGS_STYLE
        RichHelpFormatter.styles["argparse.groups"] = "bold #5fd7ff"
        RichHelpFormatter.styles["argparse.metavar"] = "deep_sky_blue1"
        RichHelpFormatter.styles["argparse.prog"] = "bold cyan"
        RichHelpFormatter.usage_markup = True

    def __init__(
        self,
        *,
        prog: str | None = None,
        usage: str | None = None,
        description: str | None = None,
        epilog: str | None = None,
        parents: Sequence[argparse.ArgumentParser] = [],
        prefix_chars: str = "-",
        fromfile_prefix_chars: str | None = None,
        argument_default: Any = None,
        conflict_handler: str = "error",
        add_help: bool = True,
        allow_abbrev: bool = True,
        exit_on_error: bool = False,
        **kwargs: Any,
    ) -> None:
        super().__init__(
            prog=prog,
            usage=usage,
            description=description,
            epilog=epilog,
            parents=parents,
            formatter_class=RichHelpFormatter,
            prefix_chars=prefix_chars,
            fromfile_prefix_chars=fromfile_prefix_chars,
            argument_default=argument_default,
            conflict_handler=conflict_handler,
            add_help=add_help,
            allow_abbrev=allow_abbrev,
            exit_on_error=exit_on_error,
        )
        _ = kwargs
        self.subcommands: list[str] = []

    @classmethod
    def subcommand(cls, *, name: str, description: str, with_help: bool = True) -> CliParser:
        """
        Return a new parser for a subcommand.

        :param name: Subcommand name.
        :param description: Short description of the subcommand.
        :param with_help: Whether to include help option.
        :return: Configured subcommand parser.
        """

        return CliParser(prog=name, description=description, add_help=with_help)

    @classmethod
    def main(cls) -> CliParser:
        """
        Return a new parser for the main program.

        :return: Main CLI parser.
        """

        return CliParser(description="Duckling's package manager")

    def add_version(self) -> CliParser:
        """
        Add a ``--version`` flag showing program version.

        :return: Self for chaining.
        """

        self.add_argument("-V", "--version", action="version", version="%(prog)s v0.1.0")
        return self

    def add_flag(
        self, *, long_name: str, short_name: str | None = None, default: bool = True, help: str
    ) -> CliParser:
        """
        Add a boolean flag option.

        :param long_name: Long flag name, e.g. ``'--verbose'``.
        :param short_name: Optional short flag name, e.g. ``'-v'``.
        :param default: Default flag state (True means flag stores True when passed).
        :param help: Help message.
        :return: Self for chaining.
        """

        options = [long_name]
        if short_name is not None:
            options.append(short_name)
        self.add_argument(*options, action="store_true" if default else "store_false", help=help)
        return self

    def add_path(
        self,
        *,
        long_name: str,
        short_name: str | None = None,
        help: str,
        default: Path | None = None,
        argument_count: ArgumentCount | None = None,
    ) -> CliParser:
        """
        Add a path argument.

        :param long_name: Long option name.
        :param short_name: Optional short name.
        :param help: Help message.
        :param default: Optional default value.
        :param argument_count: Argument count specifier.
        :return: Self for chaining.
        """

        options = [long_name]
        if short_name is not None:
            options.append(short_name)
        extras: dict[str, Any] = {"help": help}
        if default is not None:
            extras["default"] = default
            if argument_count is None:
                argument_count = ArgumentCount.Optional
            assert argument_count is ArgumentCount.Optional, (
                "only pass 'default' with 'argument_count=ArgumentCount.Optional'"
            )
        if argument_count is not None:
            extras["nargs"] = str(argument_count)
        extras["type"] = Path
        self.add_argument(*options, **extras)
        return self

    def add_str(
        self,
        *,
        long_name: str,
        short_name: str | None = None,
        help: str,
        default: str | None = None,
        argument_count: ArgumentCount | None = None,
    ) -> CliParser:
        """
        Add a string argument.

        :param long_name: Long option name.
        :param short_name: Optional short name.
        :param help: Help message.
        :param default: Optional default value.
        :param argument_count: Argument count specifier.
        :return: Self for chaining.
        """

        options = [long_name]
        if short_name is not None:
            options.append(short_name)
        extras: dict[str, Any] = {"help": help}
        if default is not None:
            if argument_count is None:
                argument_count = ArgumentCount.Optional
            assert argument_count is ArgumentCount.Optional, (
                "only pass 'default' with 'argument_count=ArgumentCount.Optional'"
            )
            extras["default"] = default
        if argument_count is not None:
            extras["nargs"] = str(argument_count)
        extras["type"] = str
        self.add_argument(*options, **extras)
        return self

    def add_int(
        self, *, long_name: str, short_name: str | None = None, help: str, default: int | None = None
    ) -> CliParser:
        """
        Add an integer argument.

        :param long_name: Long option name.
        :param short_name: Optional short name.
        :param help: Help message.
        :param default: Optional default value.
        :return: Self for chaining.
        """

        options = [long_name]
        if short_name is not None:
            options.append(short_name)
        extras: dict[str, Any] = {"help": help}
        if default is not None:
            extras["default"] = default
            extras["nargs"] = str(ArgumentCount.Optional)
        extras["type"] = int
        self.add_argument(*options, **extras)
        return self

    def add_jobs(self) -> CliParser:
        """
        Add a ``--jobs`` option for number of threads.

        :return: Self for chaining.
        """

        return self.add_int(
            long_name="--jobs",
            short_name="-j",
            help="Specify number of threads to run on",
            default=get_logical_threads(),
        )

    def add_colors(self) -> CliParser:
        """
        Add a ``--color`` option controlling output colors.

        :return: Self for chaining.
        """

        self.add_argument(
            "--color",
            choices=("auto", "never", "always"),
            type=str,
            default="auto",
            help="Control colored output",
        )
        return self

    def add_subcommands(
        self,
        *subparsers: CliParser,
        title: str,
        destination: str,
        required: bool = True,
        prog: str | None = None,
        aliases: defaultdict[str, list[str]] | None = None,
        alias_set_default_workaround: bool = False,
    ) -> CliParser:
        """
        Add multiple subcommands to this parser.

        :param subparsers: Subcommand parsers.
        :param title: Title of subcommand group.
        :param destination: Attribute to store chosen subcommand.
        :param required: Whether subcommand is required.
        :param prog: Optional program name override.
        :param aliases: Mapping from command to aliases.
        :param alias_set_default_workaround: Workaround flag for default aliases.
        :return: Self for chaining.
        """

        aliases = aliases or defaultdict(list)
        s = self.add_subparsers(title=title, dest=destination, metavar="", required=required, prog=prog)
        for subparser in subparsers:
            p = s.add_parser(
                subparser.prog,
                help=subparser.description,
                aliases=aliases[subparser.prog],
                parents=[cast(Self, subparser)],  # xD Pyright.
                formatter_class=RichHelpFormatter,
                exit_on_error=False,
                add_help=False,
            )
            if alias_set_default_workaround:
                defaults = {destination: subparser.prog}
                p.set_defaults(**defaults)
            self.subcommands.append(subparser.prog)
        return self

    def add_exclusive_group(self, *, group: ExclusiveGroup, required: bool = False) -> CliParser:
        """
        Add mutually exclusive group of options.

        :param group: Group of exclusive options.
        :param required: Whether one option is required.
        :return: Self for chaining.
        """

        g = self.add_mutually_exclusive_group(required=required)
        for arg in group.args:
            opts = [arg.long_name]
            if arg.short_name is not None:
                opts.append(arg.short_name)
            g.add_argument(*opts, **arg.kw_args_for_add_argument())
        return self

    def add_duckc(self) -> CliParser:
        """
        Add a ``--duckc`` argument to specify Duckling binary path.

        :return: Self for chaining.
        """

        return self.add_str(
            long_name="--duckc", default=DEFAULT_DUCKC_BINARY, help="Change path of the Duckling binary"
        )

    def add_chdir(self) -> CliParser:
        """
        Add a ``--directory`` argument to change working directory before actions.

        :return: Self for chaining.
        """

        return self.add_path(
            long_name="--directory", short_name="-C", help="Change to DIRECTORY before performing any actions"
        )


@dataclass(frozen=True, kw_only=True)
class BooleanInfo:
    """
    Storage for a boolean CLI flag definition.
    """

    long_name: str
    short_name: str | None
    default: bool
    help: str

    def kw_args_for_add_argument(self) -> dict[str, Any]:
        """
        Return keyword args for ``argparse.add_argument()`` for this flag.
        """

        return {"action": "store_true" if self.default else "store_false", "help": self.help}


@dataclass(frozen=True, kw_only=True)
class Info:
    """
    Storage for any non-trivial argument.
    """

    long_name: str
    short_name: str | None
    default: Any | None
    help: str
    type: type
    count: ArgumentCount | None = None

    def kw_args_for_add_argument(self) -> dict[str, Any]:
        """
        Return keyword args for ``argparse.add_argument()`` for this flag.
        """

        extras: dict[str, Any] = {"help": self.help}
        count = self.count
        if self.default is not None:
            if count is None:
                count = ArgumentCount.Optional
            assert count is ArgumentCount.Optional, (
                "only pass 'default' with 'argument_count=ArgumentCount.Optional'"
            )
            assert isinstance(self.default, self.type), "'default' and 'type' mismatch"
            extras["default"] = self.default
        if count is not None:
            extras["nargs"] = str(count)
        extras["type"] = self.type
        return extras


class ExclusiveGroup:
    """
    Helper to define a mutually exclusive group of CLI options.
    """

    def __init__(self) -> None:
        self.args: list[BooleanInfo | Info] = []

    def add_flag(
        self, *, long_name: str, short_name: str | None = None, default: bool = True, help: str
    ) -> ExclusiveGroup:
        """
        Add a boolean flag to this exclusive group.

        :return: Self for chaining.
        """

        self.args.append(BooleanInfo(long_name=long_name, short_name=short_name, default=default, help=help))
        return self

    def add_quiet(self) -> ExclusiveGroup:
        """
        Add a ``--quiet`` flag.

        :return: Self for chaining.
        """

        return self.add_flag(long_name="--quiet", short_name="-q", help="Supress all output")

    def add_verbose(self) -> ExclusiveGroup:
        """
        Add a ``--verbose`` flag.

        :return: Self for chaining.
        """

        return self.add_flag(long_name="--verbose", short_name="-v", help="Use verbose output")

    def add_profile(self) -> ExclusiveGroup:
        """
        Add a ``--profile`` argument.

        :return: Self for chaining.
        """

        self.args.append(
            Info(
                long_name="--profile",
                short_name=None,
                default=DEFAULT_BUILD_PROFILE,
                type=str,
                help="Select build profile",
            )
        )
        return self

    def add_release(self) -> ExclusiveGroup:
        """
        Add a `--release` flag as alias for ``--profile=release``.

        :return: Self for chaining.
        """

        return self.add_flag(long_name="--release", help=f"Alias for [{ARGS_STYLE}]--profile=release[/]")

    def add_str(
        self,
        *,
        long_name: str,
        short_name: str | None = None,
        help: str,
        default: str | None = None,
        argument_count: ArgumentCount | None = None,
    ) -> ExclusiveGroup:
        """
        Add a string argument to this exclusive group.

        :param long_name: Long option name.
        :param short_name: Optional short name.
        :param help: Help message.
        :param default: Optional default value.
        :param argument_count: Argument count specifier.
        :return: Self for chaining.
        """

        self.args.append(
            Info(
                long_name=long_name,
                short_name=short_name,
                default=default,
                help=help,
                type=str,
                count=argument_count,
            )
        )
        return self
