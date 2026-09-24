import re
import threading
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

from .resource_manager import ResourceManager
from .reporting import CompletionOutput, OrderedOutput


@dataclass
class RunContext:
    """
    Options and shared state of a single `itest` invocation.
    """

    # Matched with `re.search` against full `node/.../test/case` paths.
    pattern: re.Pattern
    clean: bool
    dry: bool
    fail_fast: bool
    verbose: bool
    log_file: Path
    # Budget of machine threads: a case holds its `NeededThreads` for as
    # long as it runs, so this bounds the threads in flight, not the
    # cases. Ignored when `sequential`.
    jobs: int
    # Run one case at a time, with no thread budget and no `NeededThreads`
    # accounting at all. Forced by `--dry` and `--clean`.
    sequential: bool
    # Keep core dumps enabled for test commands (they are disabled by
    # default; see `dit_exec_command`).
    core_dumps: bool
    # Multiplies every resolved `TimeOut`.
    timeout_scale: float
    output: CompletionOutput | OrderedOutput
    # List only what did not pass: passing cases are the bulk of the output and
    # carry the least information. Sections are then assembled per test rather
    # than streamed, so a header is printed only when its test has something to
    # report.
    quiet: bool = False
    # Executes the group-start and case tasks; None when `sequential`
    # (groups then run inline on the calling thread).
    pool: ThreadPoolExecutor | None = None
    # Grants each running case its `NeededThreads`, capping the threads
    # in flight across all cases at `jobs`; None when `sequential`.
    threads: ResourceManager | None = None
    abort: threading.Event = field(default_factory=threading.Event)
    # Failures of the `PreNode`/`PostNode`/`PreTest`/`PostTest` hooks, as
    # (path, kind) pairs. Kept apart from the case statistics so that a
    # failing hook is not counted as a failed case; folded into the verdict
    # but never into the case counts.
    node_failures: list[tuple[str, str]] = field(
        default_factory=list[tuple[str, str]]
    )
    node_failures_lock: threading.Lock = field(default_factory=threading.Lock)
    # Cases that never got to run, by reason (fail-fast and the hooks above).
    # Counted so that skipped work is visible in the summary instead of being
    # silently absent from it.
    not_run: dict[str, int] = field(default_factory=dict[str, int])
    not_run_lock: threading.Lock = field(default_factory=threading.Lock)

    def record_node_failure(self, path: str, kind: str):
        with self.node_failures_lock:
            self.node_failures.append((path, kind))

    def record_not_run(self, reason: str, count: int):
        if count <= 0:
            return
        with self.not_run_lock:
            self.not_run[reason] = self.not_run.get(reason, 0) + count

    @property
    def not_run_total(self) -> int:
        return sum(self.not_run.values())

    @property
    def parallel(self) -> bool:
        return not self.sequential

    @property
    def streaming(self) -> bool:
        """
        Whether case output is printed live instead of buffered into
        per-test sections.
        """
        return not self.parallel

    @property
    def streamed_sections(self) -> bool:
        """
        Whether a test's section is printed as its cases run, which is what
        `--sequential` does. `--quiet` has to see a whole test before it can
        tell whether that test is worth a header, so it buffers instead.
        """
        return self.streaming and not self.quiet
