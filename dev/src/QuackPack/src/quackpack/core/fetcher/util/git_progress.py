from typing import Any, Final, override

from git import RemoteProgress
from rich import console, progress


class GitRemoteProgress(RemoteProgress):
    OP_CODES: Final[list[str]] = [
        "BEGIN",
        "CHECKING_OUT",
        "COMPRESSING",
        "COUNTING",
        "END",
        "FINDING_SOURCES",
        "RECEIVING",
        "RESOLVING",
        "WRITING",
    ]
    OP_CODE_MAP: Final[dict[Any, str]] = {
        getattr(RemoteProgress, _op_code): _op_code for _op_code in OP_CODES
    }

    def __init__(self, progress: progress.Progress) -> None:
        super().__init__()
        self.progress = progress
        self.progress.start()
        self.active_task = None

    def __del__(self) -> None:
        self.progress.stop()

    @staticmethod
    def make_git_progress(console: console.Console) -> progress.Progress:
        return progress.Progress(
            progress.SpinnerColumn(),
            # *progress.Progress.get_default_columns(),
            progress.TextColumn("[progress.description]{task.description}"),
            progress.BarColumn(),
            progress.TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
            "eta",
            progress.TimeRemainingColumn(),
            progress.TextColumn("{task.fields[message]}"),
            console=console,
            transient=True,
        )

    @classmethod
    def get_curr_op(cls, op_code: int) -> str:
        """Get OP name from OP code."""
        # Remove BEGIN- and END-flag and get op name
        op_code_masked = op_code & cls.OP_MASK
        return cls.OP_CODE_MAP.get(op_code_masked, "?").title()

    @override
    def update(
        self,
        op_code: int,
        cur_count: str | float,
        max_count: str | float | None = None,
        message: str | None = "",
    ) -> None:
        # Start new bar on each BEGIN-flag
        if op_code & self.BEGIN:
            self.curr_op = self.get_curr_op(op_code)
            self.active_task = self.progress.add_task(
                description=self.curr_op, total=float(max_count or 0), message=message
            )

        assert self.active_task is not None, "update() called without BEGIN"
        self.progress.update(
            task_id=self.active_task,
            completed=float(cur_count),
            message=message,
            refresh=True,
        )

        # End progress monitoring on each END-flag
        if op_code & self.END:
            self.progress.update(
                task_id=self.active_task, message=f"[bright_black]{message}"
            )
