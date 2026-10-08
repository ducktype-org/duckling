# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import json
import re
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from .context import RunContext
from .keys import NEEDED_THREADS
from .reporting import CompletionOutput, OrderedOutput, print_success
from .resource_manager import ResourceManager
from .scheduler import TestGroup, run_groups, sweep
from .test_loader import load_tests
from ...commands.helpers import get_cpu_count
from ..helpers import (
    exit_with_error,
    get_dev_directory,
    get_input,
    log_info,
    log_warning,
)

DEFAULT_LOG_FILE_PATH = Path("/tmp/dit.log")


def tester_impl(
    clean: bool,
    dry: bool,
    filter: str,
    fail_fast: bool,
    verbose: bool,
    log_file: str | Path,
    build_dir: str,
    determinism_check: bool | None = None,
    custom_values: str = "{}",
    jobs: int = get_cpu_count(),
    sequential: bool = False,
    deterministic_output: bool = False,
    core_dumps: bool = False,
    timeout_scale: float = 1.0,
):
    """
    The driver function of Duckling Integration Tests framework.
    """
    log_info("Running integration tests...")

    log_file = _prepare_log_file(Path(log_file))

    try:
        pattern = re.compile(filter)
    except re.error as e:
        exit_with_error(f"Invalid `-t` filter regex `{filter}`: {e}")

    user_values = json.loads(custom_values)
    user_values["build_dir"] = str(Path(build_dir).absolute())
    user_values["dev_dir"] = str(get_dev_directory())
    if determinism_check is not None:
        user_values["enable_conc_deterministic_tests"] = str(determinism_check).lower()

    test_set = load_tests("integration_tests", user_values=user_values)

    # `--dry` and `--clean` keep the deterministic, sequential order.
    if dry or clean:
        sequential = True
    jobs = max(1, jobs)

    if jobs == 1 and not sequential:
        log_warning(
            "`-j 1` is a budget of one thread, not a sequential run: cases still go"
            " through the scheduler, their output is buffered into per-test sections,"
            " and any case needing more than one thread is rejected."
            " Did you mean `--sequential`?"
        )

    ctx = RunContext(
        pattern=pattern,
        clean=clean,
        dry=dry,
        fail_fast=fail_fast,
        verbose=verbose,
        log_file=log_file,
        jobs=jobs,
        sequential=sequential,
        core_dumps=core_dumps,
        timeout_scale=timeout_scale,
        output=CompletionOutput(),
    )

    normal, deferred = sweep(test_set, ctx)

    # When sequential, the completion order is the tree order already.
    if ctx.parallel and deterministic_output:
        output = OrderedOutput()
        for group in normal + deferred:
            output.register(group.path)
        ctx.output = output
    if ctx.parallel:
        _check_thread_budget(normal, ctx)
        ctx.pool = ThreadPoolExecutor(max_workers=jobs, thread_name_prefix="dit-case")
        ctx.threads = ResourceManager(max_resources=jobs)

    try:
        stats = run_groups(normal, deferred, ctx)
    finally:
        if ctx.pool is not None:
            ctx.pool.shutdown()
        ctx.output.drain()

    if dry:
        return

    succeeded, failed, disabled = stats
    total_test_count = len(succeeded) + len(failed) + len(disabled)
    print(f"Ran test count: {total_test_count}")
    print(f" - Succeeded: {len(succeeded)}")
    print(f" - Disabled:  {len(disabled)}")
    print(f" - Failed:    {len(failed)}")

    if len(failed):
        failed_tests = map(lambda x: " - " + x, failed)
        exit_with_error(
            f"{'(Fail fast) ' if fail_fast else ''}Failed tests:\n{'\n'.join(failed_tests)}\n"
            + f"Please see log file '{log_file.absolute()}' for more info."
        )
    elif not clean:
        print_success(f"All tests have run successfully!")


def _check_thread_budget(groups: list[TestGroup], ctx: RunContext):
    """
    Rejects cases asking for more threads than the whole run was given.
    Such a request can never be granted, and the scheduler must not
    silently run the case on fewer threads than it declared.

    Only the concurrently scheduled groups are checked; `NoParallel`
    ones run one case at a time anyway, as does the `--sequential` mode
    (which skips this check entirely).
    """
    too_wide = [
        f"{group.path}/{case.name} needs {case.needed_threads}"
        for group in groups
        for _, case in group.cases
        if case.needed_threads > ctx.jobs
    ]
    if too_wide:
        listing = "\n".join(f" - {entry}" for entry in too_wide)
        exit_with_error(
            f"`{NEEDED_THREADS}` of {len(too_wide)} case(s) exceeds the"
            f" `-j {ctx.jobs}` thread budget:\n{listing}\n"
            f"Raise `-j`, or pass `--sequential` to run one case at a time."
        )


def _prepare_log_file(log_file: Path) -> Path:
    if log_file.exists():
        if log_file.absolute() != DEFAULT_LOG_FILE_PATH and log_file.stat().st_size > 0:
            log_warning(
                f"File: {log_file} already exists and is NOT empty! It will be overwritten!"
            )
            inp = get_input(f"Continue anyway: [y/N] ").lower()
            if inp not in ["y"]:
                exit_with_error("Exiting...")
        log_file.unlink()
    return log_file
