import re
import threading
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

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
    jobs: int
    output: CompletionOutput | OrderedOutput
    # Executes the group-start and case tasks; None when `jobs == 1`
    # (groups then run inline on the calling thread).
    pool: ThreadPoolExecutor | None = None
    abort: threading.Event = field(default_factory=threading.Event)
    # Paths of PostNode commands that failed; folded into the final
    # statistics (the tests beneath keep their own results).
    node_failures: list[str] = field(default_factory=list[str])
    node_failures_lock: threading.Lock = field(default_factory=threading.Lock)

    @property
    def parallel(self) -> bool:
        return self.jobs > 1

    @property
    def streaming(self) -> bool:
        """
        Whether case output is printed live instead of buffered into
        per-test sections.
        """
        return not self.parallel
