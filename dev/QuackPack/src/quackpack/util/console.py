from collections.abc import Callable, Mapping
from datetime import datetime
from typing import IO, Any, Literal, override

from rich.console import Console as RichConsole
from rich.emoji import EmojiVariant
from rich.highlighter import ReprHighlighter
from rich.style import Style, StyleType
from rich.text import Text
from rich.theme import Theme

# These are copied one-to-one from `rich/console.py`.
type FormatTimeCallable = Callable[[datetime], Text]
type HighlighterType = Callable[[str | Text], Text]
type JustifyMethod = Literal["default", "left", "center", "right", "full"]
type OverflowMethod = Literal["fold", "crop", "ellipsis", "ignore"]

__all__ = ["Console"]


class Console(RichConsole):
    """
    Our wrapper on RichConsole.
    """

    _verbose: bool = False
    """
    Controls, whether verbose messages should be printed.
    """

    def __init__(
        self,
        *,
        color_system: Literal["auto", "standard", "256", "truecolor", "windows"] | None = "auto",
        force_terminal: bool | None = None,
        force_jupyter: bool | None = None,
        force_interactive: bool | None = None,
        soft_wrap: bool = False,
        theme: Theme | None = None,
        stderr: bool = False,
        file: IO[str] | None = None,
        quiet: bool = False,
        width: int | None = None,
        height: int | None = None,
        style: StyleType | None = None,
        no_color: bool | None = None,
        tab_size: int = 8,
        record: bool = False,
        markup: bool = True,
        emoji: bool = True,
        emoji_variant: EmojiVariant | None = None,
        highlight: bool = True,
        log_time: bool = True,
        log_path: bool = True,
        log_time_format: str | FormatTimeCallable = "[%X]",
        highlighter: HighlighterType | None = ReprHighlighter(),  # noqa: B008, silence ruff warnings, as we're just passing everything up.
        legacy_windows: bool | None = None,
        safe_box: bool = True,
        get_datetime: Callable[[], datetime] | None = None,
        get_time: Callable[[], float] | None = None,
        _environ: Mapping[str, str] | None = None,
    ):
        """
        This method just passes arguments to rich.console.Console constructor, look at their documentation.
        """
        super().__init__(
            color_system=color_system,
            force_terminal=force_terminal,
            force_jupyter=force_jupyter,
            force_interactive=force_interactive,
            soft_wrap=soft_wrap,
            theme=theme,
            stderr=stderr,
            file=file,
            quiet=quiet,
            width=width,
            height=height,
            style=style,
            no_color=no_color,
            tab_size=tab_size,
            record=record,
            markup=markup,
            emoji=emoji,
            emoji_variant=emoji_variant,
            highlight=highlight,
            log_time=log_time,
            log_path=log_path,
            log_time_format=log_time_format,
            highlighter=highlighter,
            legacy_windows=legacy_windows,
            safe_box=safe_box,
            get_datetime=get_datetime,
            get_time=get_time,
            _environ=_environ,
        )

    def set_verbose(self) -> None:
        """
        Mark this console as verbose.
        """
        self._verbose = True

    # TODO: Should this be marked `@override`, if we add default value?
    @override
    def print(
        self,
        *objects: Any,
        sep: str = " ",
        end: str = "\n",
        style: str | Style | None = None,
        justify: JustifyMethod | None = None,
        overflow: OverflowMethod | None = None,
        no_wrap: bool | None = None,
        emoji: bool | None = None,
        markup: bool | None = None,
        highlight: bool | None = None,
        width: int | None = None,
        height: int | None = None,
        crop: bool = True,
        soft_wrap: bool | None = None,
        new_line_start: bool = False,
        verbose: bool = False,
    ) -> None:
        """
        Our wrapper for `print()`, which also checks `verbose` flag.
        """
        if verbose and not self._verbose:
            return
        super().print(
            *objects,
            sep=sep,
            end=end,
            style=style,
            justify=justify,
            overflow=overflow,
            no_wrap=no_wrap,
            emoji=emoji,
            markup=markup,
            highlight=highlight,
            width=width,
            height=height,
            crop=crop,
            soft_wrap=soft_wrap,
            new_line_start=new_line_start,
        )

    def error(
        self,
        *objects: Any,
        sep: str = " ",
        end: str = "\n",
        style: str | Style | None = None,
        justify: JustifyMethod | None = None,
        overflow: OverflowMethod | None = None,
        no_wrap: bool | None = None,
        emoji: bool | None = None,
        markup: bool | None = None,
        highlight: bool | None = None,
        width: int | None = None,
        height: int | None = None,
        crop: bool = True,
        soft_wrap: bool | None = None,
        new_line_start: bool = False,
        verbose: bool = False,
    ) -> None:
        """
        Helper for putting `Error` in bold red before text.
        """
        self.print(
            "[bold red]Error:",
            *objects,
            sep=sep,
            end=end,
            style=style,
            justify=justify,
            overflow=overflow,
            no_wrap=no_wrap,
            emoji=emoji,
            markup=markup,
            highlight=highlight,
            width=width,
            height=height,
            crop=crop,
            soft_wrap=soft_wrap,
            new_line_start=new_line_start,
            verbose=verbose,
        )

    def warning(
        self,
        *objects: Any,
        sep: str = " ",
        end: str = "\n",
        style: str | Style | None = None,
        justify: JustifyMethod | None = None,
        overflow: OverflowMethod | None = None,
        no_wrap: bool | None = None,
        emoji: bool | None = None,
        markup: bool | None = None,
        highlight: bool | None = None,
        width: int | None = None,
        height: int | None = None,
        crop: bool = True,
        soft_wrap: bool | None = None,
        new_line_start: bool = False,
        verbose: bool = False,
    ) -> None:
        """
        Helper for putting `Warning` in bold yellow before text.
        """
        self.print(
            "[bold yellow]Warning:",
            *objects,
            sep=sep,
            end=end,
            style=style,
            justify=justify,
            overflow=overflow,
            no_wrap=no_wrap,
            emoji=emoji,
            markup=markup,
            highlight=highlight,
            width=width,
            height=height,
            crop=crop,
            soft_wrap=soft_wrap,
            new_line_start=new_line_start,
            verbose=verbose,
        )

    def critical(
        self,
        *objects: Any,
        sep: str = " ",
        end: str = "\n",
        style: str | Style | None = None,
        justify: JustifyMethod | None = None,
        overflow: OverflowMethod | None = None,
        no_wrap: bool | None = None,
        emoji: bool | None = None,
        markup: bool | None = None,
        highlight: bool | None = None,
        width: int | None = None,
        height: int | None = None,
        crop: bool = True,
        soft_wrap: bool | None = None,
        new_line_start: bool = False,
        verbose: bool = False,
    ) -> None:
        """
        Helper for putting `Critical` in bold reversed-red before text.
        """
        self.print(
            "[bold red reverse]Critical:",
            *objects,
            sep=sep,
            end=end,
            style=style,
            justify=justify,
            overflow=overflow,
            no_wrap=no_wrap,
            emoji=emoji,
            markup=markup,
            highlight=highlight,
            width=width,
            height=height,
            crop=crop,
            soft_wrap=soft_wrap,
            new_line_start=new_line_start,
            verbose=verbose,
        )

    def info(
        self,
        *objects: Any,
        sep: str = " ",
        end: str = "\n",
        style: str | Style | None = None,
        justify: JustifyMethod | None = None,
        overflow: OverflowMethod | None = None,
        no_wrap: bool | None = None,
        emoji: bool | None = None,
        markup: bool | None = None,
        highlight: bool | None = None,
        width: int | None = None,
        height: int | None = None,
        crop: bool = True,
        soft_wrap: bool | None = None,
        new_line_start: bool = False,
        verbose: bool = False,
    ) -> None:
        """
        Helper for putting `Info` in bold cyan before text.
        """
        self.print(
            "[bold cyan]Info:",
            *objects,
            sep=sep,
            end=end,
            style=style,
            justify=justify,
            overflow=overflow,
            no_wrap=no_wrap,
            emoji=emoji,
            markup=markup,
            highlight=highlight,
            width=width,
            height=height,
            crop=crop,
            soft_wrap=soft_wrap,
            new_line_start=new_line_start,
            verbose=verbose,
        )

    def debug(
        self,
        *objects: Any,
        sep: str = " ",
        end: str = "\n",
        style: str | Style | None = None,
        justify: JustifyMethod | None = None,
        overflow: OverflowMethod | None = None,
        no_wrap: bool | None = None,
        emoji: bool | None = None,
        markup: bool | None = None,
        highlight: bool | None = None,
        width: int | None = None,
        height: int | None = None,
        crop: bool = True,
        soft_wrap: bool | None = None,
        new_line_start: bool = False,
        verbose: bool = False,
    ) -> None:
        """
        Helper for putting `Debug` in bold cyan before text.
        """
        self.print(
            "[bold cyan]Debug:",
            *objects,
            sep=sep,
            end=end,
            style=style,
            justify=justify,
            overflow=overflow,
            no_wrap=no_wrap,
            emoji=emoji,
            markup=markup,
            highlight=highlight,
            width=width,
            height=height,
            crop=crop,
            soft_wrap=soft_wrap,
            new_line_start=new_line_start,
            verbose=verbose,
        )

    def warn(
        self,
        *objects: Any,
        sep: str = " ",
        end: str = "\n",
        style: str | Style | None = None,
        justify: JustifyMethod | None = None,
        overflow: OverflowMethod | None = None,
        no_wrap: bool | None = None,
        emoji: bool | None = None,
        markup: bool | None = None,
        highlight: bool | None = None,
        width: int | None = None,
        height: int | None = None,
        crop: bool = True,
        soft_wrap: bool | None = None,
        new_line_start: bool = False,
        verbose: bool = False,
    ) -> None:
        """
        Alias for `warning()`.
        """
        self.warning(
            *objects,
            sep=sep,
            end=end,
            style=style,
            justify=justify,
            overflow=overflow,
            no_wrap=no_wrap,
            emoji=emoji,
            markup=markup,
            highlight=highlight,
            width=width,
            height=height,
            crop=crop,
            soft_wrap=soft_wrap,
            new_line_start=new_line_start,
            verbose=verbose,
        )
