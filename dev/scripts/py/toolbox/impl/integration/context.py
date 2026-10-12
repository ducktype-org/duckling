# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

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
    # Executes the group-start and case tasks; None when `sequential`
    # (groups then run inline on the calling thread).
    pool: ThreadPoolExecutor | None = None
    # Grants each running case its `NeededThreads`, capping the threads
    # in flight across all cases at `jobs`; None when `sequential`.
    threads: ResourceManager | None = None
    abort: threading.Event = field(default_factory=threading.Event)
    # Paths of PostNode commands that failed; folded into the final
    # statistics (the tests beneath keep their own results).
    node_failures: list[str] = field(default_factory=list[str])
    node_failures_lock: threading.Lock = field(default_factory=threading.Lock)

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
