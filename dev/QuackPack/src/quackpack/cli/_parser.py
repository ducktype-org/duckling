from __future__ import annotations

import argparse
from collections import defaultdict
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Final, Self, cast

from rich_argparse import RichHelpFormatter

from quackpack.util.cpu import get_logical_threads
from quackpack.util.global_context import DEFAULT_BUILD_PROFILE, DEFAULT_DUCKC_BINARY

ARGS_STYLE: Final[str] = "bold cyan"


class CliParser(argparse.ArgumentParser):
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
        Return a new subcommand.
        """
        return CliParser(prog=name, description=description, add_help=with_help)

    @classmethod
    def main(cls) -> CliParser:
        """
        Return a new main program.
        """
        return CliParser(description="Duckling's package manager")

    def add_version(self) -> CliParser:
        """
        Add a version argument.
        """
        self.add_argument("-V", "--version", action="version", version="%(prog)s v0.1.0")
        return self

    def add_flag(
        self, *, long_name: str, short_name: str | None = None, default: bool = True, help: str
    ) -> CliParser:
        """
        Add a boolean flag.
        """
        options = [long_name]
        if short_name is not None:
            options.append(short_name)
        self.add_argument(*options, action="store_true" if default else "store_false", help=help)
        return self

    def add_path(
        self, *, long_name: str, short_name: str | None = None, help: str, default: Path | None = None
    ) -> CliParser:
        """
        Add a `Path` argument.
        """
        options = [long_name]
        if short_name is not None:
            options.append(short_name)
        extras: dict[str, Any] = {"help": help}
        if default is not None:
            extras["default"] = default
            extras["nargs"] = "?"
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
        multiple: bool = False,
    ) -> CliParser:
        """
        Add a `str` argument.
        """
        options = [long_name]
        if short_name is not None:
            options.append(short_name)
        extras: dict[str, Any] = {"help": help}
        if default is not None:
            extras["default"] = default
            assert not multiple, "don't pass 'multiple=True' with 'default'"
            extras["nargs"] = "?"
        elif multiple:
            extras["nargs"] = "+"
        extras["type"] = str
        self.add_argument(*options, **extras)
        return self

    def add_int(
        self,
        *,
        long_name: str,
        short_name: str | None = None,
        help: str,
        default: int | None = None,
        multiple: bool = False,
    ) -> CliParser:
        """
        Add an `int` argument.
        """
        options = [long_name]
        if short_name is not None:
            options.append(short_name)
        extras: dict[str, Any] = {"help": help}
        if default is not None:
            extras["default"] = default
            assert not multiple, "don't pass 'multiple=True' with 'default'"
            extras["nargs"] = "?"
        elif multiple:
            extras["nargs"] = "+"
        extras["type"] = int
        self.add_argument(*options, **extras)
        return self

    def add_jobs(self) -> CliParser:
        """
        Add `--jobs` argument.
        """
        return self.add_int(
            long_name="--jobs",
            short_name="-j",
            help="Specify number of threads to run on",
            default=get_logical_threads(),
        )

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
        Add subcommands for this parser.
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
                defaults = {}
                defaults[destination] = subparser.prog
                p.set_defaults(**defaults)
            self.subcommands.append(subparser.prog)
        return self

    def add_exclusive_group(self, *, group: ExclusiveGroup, required: bool = False) -> CliParser:
        """
        Add mutually exclusive arguments.
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
        Add a `--duckc` argument.
        """
        return self.add_str(
            long_name="--duckc", default=DEFAULT_DUCKC_BINARY, help="Change path of the Duckling binary"
        )

    def add_chdir(self) -> CliParser:
        """
        Add a `--directory` argument.
        """
        return self.add_path(
            long_name="--directory",
            short_name="-C",
            default=Path.cwd(),
            help="Change to DIRECTORY before performing any actions",
        )


@dataclass(frozen=True, kw_only=True)
class BooleanInfo:
    """
    Storage for the boolean flag.
    """

    long_name: str
    short_name: str | None
    default: bool
    help: str

    def kw_args_for_add_argument(self) -> dict[str, Any]:
        """
        Get the keyword arguments for `add_argument()`.
        """
        return {"action": "store_true" if self.default else "store_false", "help": self.help}


@dataclass(frozen=True, kw_only=True)
class Info:
    """
    Storage for any non trivial argument.
    """

    long_name: str
    short_name: str | None
    default: Any | None
    help: str
    type: type
    multiple: bool

    def kw_args_for_add_argument(self) -> dict[str, Any]:
        """
        Get the keyword arguments for `add_argument()`.
        """
        extras: dict[str, Any] = {"help": self.help}
        if self.default is not None:
            extras["default"] = self.default
            assert not self.multiple, "don't pass 'multiple=True' with 'default'"
            extras["nargs"] = "?"
        elif self.multiple:
            extras["nargs"] = "+"
        extras["type"] = self.type
        return extras


class ExclusiveGroup:
    def __init__(self) -> None:
        self.args: list[BooleanInfo | Info] = []

    def add_flag(
        self, *, long_name: str, short_name: str | None = None, default: bool = True, help: str
    ) -> ExclusiveGroup:
        """
        Add a boolean flag.
        """
        self.args.append(BooleanInfo(long_name=long_name, short_name=short_name, default=default, help=help))
        return self

    def add_quiet(self) -> ExclusiveGroup:
        """
        Add the `--quiet` flag.
        """
        return self.add_flag(long_name="--quiet", short_name="-q", help="Supress all output")

    def add_verbose(self) -> ExclusiveGroup:
        """
        Add the `--verbose` flag.
        """
        return self.add_flag(long_name="--verbose", short_name="-v", help="Use verbose output")

    def add_profile(self) -> ExclusiveGroup:
        """
        Add the `--profile` argument.
        """
        self.args.append(
            Info(
                long_name="--profile",
                short_name=None,
                default=DEFAULT_BUILD_PROFILE,
                multiple=False,
                type=str,
                help="Select build profile",
            )
        )
        return self

    def add_release(self) -> ExclusiveGroup:
        """
        Add the `--release` flag.
        """
        return self.add_flag(long_name="--release", help=f"Alias for [{ARGS_STYLE}]--profile=release[/]")
