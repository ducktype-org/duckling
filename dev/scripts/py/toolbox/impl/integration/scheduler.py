import threading
from dataclasses import dataclass, field

from .classes import Case, Test, TestNode
from .context import RunContext
from .resource_manager import ResourceAcquirer
from .reporting import (
    CaseLog,
    TestStatistics,
    log_info_if_needed,
    print_failure,
    write_log,
)
from .runner import clean_test, run_single_case
from .utils import dit_exec_command
from ..helpers import log_info


class NodeOpenError(Exception):
    """
    Raised when the `PreNode` of a node (or of one of its ancestors)
    failed; every group beneath such a node fails without running.
    """

    def __init__(self, node: "NodeState"):
        super().__init__(f"PreNode of `{node.path}` has failed")
        self.node = node


@dataclass
class NodeState:
    """
    Scheduling state of one `TestNode`: `pending` counts the unfinished
    groups beneath it (deferred `NoParallel` ones included), so `PreNode`
    runs once before the first of them and `PostNode` once after the last.
    """

    node: TestNode
    path: str
    parent: "NodeState | None"
    pending: int = 0
    opened: bool = False
    open_error: NodeOpenError | None = None
    # Whether the PreNode failure has already been printed; the groups
    # beneath are all counted as failed, but the console line and the
    # log entry appear once.
    reported: bool = False
    lock: threading.Lock = field(default_factory=threading.Lock)


@dataclass
class TestGroup:
    """
    A test together with its filter-selected cases: the unit of
    scheduling and of output (a group's section prints atomically).
    """

    path: str
    test: Test
    cases: list[tuple[int, Case]]
    node: NodeState
    stats: TestStatistics = field(default_factory=lambda: TestStatistics([], [], []))
    results: list[TestStatistics] = field(default_factory=list[TestStatistics])
    case_logs: list[CaseLog] = field(default_factory=list[CaseLog])
    remaining: int = 0
    lock: threading.Lock = field(default_factory=threading.Lock)


def sweep(root: TestNode, ctx: RunContext) -> tuple[list[TestGroup], list[TestGroup]]:
    """
    Collects every test with at least one case matching the filter into
    flat, tree-ordered lists of groups: (normal, no_parallel). Builds the
    `NodeState` chain and counts each group into `pending` of all its
    ancestors.
    """
    normal: list[TestGroup] = []
    deferred: list[TestGroup] = []
    _sweep_node(root, None, ctx, normal, deferred)
    return normal, deferred


def _sweep_node(
    node: TestNode,
    parent: NodeState | None,
    ctx: RunContext,
    normal: list[TestGroup],
    deferred: list[TestGroup],
):
    path = f"{parent.path}/{node.name}" if parent else node.name
    state = NodeState(node=node, path=path, parent=parent)
    for test in node.tests:
        test_path = f"{path}/{test.name}"
        selected = [
            (i, case)
            for i, case in enumerate(test.cases)
            if ctx.pattern.search(f"{test_path}/{case.name}")
        ]
        if not selected:
            continue
        group = TestGroup(path=test_path, test=test, cases=selected, node=state)
        (deferred if test.no_parallel else normal).append(group)
        ancestor = state
        while ancestor is not None:
            ancestor.pending += 1
            ancestor = ancestor.parent
    for subtest in node.subtests:
        _sweep_node(subtest, state, ctx, normal, deferred)


def _ensure_open(state: NodeState, ctx: RunContext):
    """
    Runs the `PreNode` of `state` and of all its ancestors, exactly once,
    parents first. Concurrent callers block until the node is open; a
    stored failure is re-raised for every later group.
    """
    with state.lock:
        if state.open_error is not None:
            raise state.open_error
        if state.opened:
            return
        if state.parent is not None:
            # Lock order is strictly child -> parent, and the holder of a
            # long PreNode is itself running, so this cannot deadlock.
            _ensure_open(state.parent, ctx)
        if state.node.pre_node:
            log_info_if_needed("Executing pre-node command...", ctx.dry, ctx.verbose)
            try:
                dit_exec_command(
                    state.node.pre_node,
                    cwd=state.node.cwd,
                    capture_output=not ctx.verbose,
                    dry=ctx.dry,
                    verbose=ctx.verbose,
                    core_dumps=ctx.core_dumps,
                )
            except Exception as e:
                state.open_error = NodeOpenError(state)
                _log_failure(state.path, e, ctx)
                raise state.open_error
        state.opened = True


def _release(state: NodeState, ctx: RunContext):
    """
    Marks one group beneath `state` as finished; the last one triggers
    `PostNode` (if the node ever opened) and releases the parent.
    """
    with state.lock:
        state.pending -= 1
        last = state.pending == 0
        run_post = last and state.opened and state.open_error is None
    if not last:
        return
    if run_post and state.node.post_node:
        log_info_if_needed("Executing post-node command...", ctx.dry, ctx.verbose)
        try:
            dit_exec_command(
                state.node.post_node,
                cwd=state.node.cwd,
                capture_output=not ctx.verbose,
                dry=ctx.dry,
                verbose=ctx.verbose,
                core_dumps=ctx.core_dumps,
            )
        except Exception as e:
            ctx.output.emit_now(
                [(print_failure, f"PostNode of `{state.path}` has failed.")]
            )
            _log_failure(state.path, e, ctx)
            with ctx.node_failures_lock:
                ctx.node_failures.append(state.path)
    if state.parent is not None:
        _release(state.parent, ctx)


def _log_failure(path: str, error: Exception, ctx: RunContext):
    message = "".join(str(arg) for arg in error.args) or repr(error)
    write_log(f"{path} has failed:\n{message}\n", ctx.log_file)


def _start_group(group: TestGroup, ctx: RunContext) -> bool:
    """
    Opens the group's node chain and runs `PreTest`. Returns whether the
    cases should run; on False the group's output section has already
    been submitted (the caller still owns the node release).
    """
    group.results = [TestStatistics([], [], []) for _ in group.cases]
    group.case_logs = [
        CaseLog(ctx.log_file, immediate=ctx.streaming) for _ in group.cases
    ]
    group.remaining = len(group.cases)

    # An empty section still advances the ordered output, so skipped
    # groups do not stall the ones after them.
    if ctx.abort.is_set():
        ctx.output.submit(group.path, [])
        return False

    try:
        _ensure_open(group.node, ctx)
    except NodeOpenError as e:
        with e.node.lock:
            first = not e.node.reported
            e.node.reported = True
        if first:
            ctx.output.emit_now(
                [(print_failure, f"PreNode of `{e.node.path}` has failed.")]
            )
        group.stats.failed.append(group.path)
        ctx.output.submit(group.path, [])
        return False

    header = (log_info, f"===== {group.path} =====")
    if ctx.streaming:
        ctx.output.submit(group.path, [header])

    if group.test.pre_test:
        log_info_if_needed("Executing pre-test...", ctx.dry, ctx.verbose)
        try:
            dit_exec_command(
                group.test.pre_test,
                cwd=group.test.cwd,
                capture_output=not ctx.verbose,
                dry=ctx.dry,
                verbose=ctx.verbose,
                core_dumps=ctx.core_dumps,
            )
        except Exception as e:
            group.stats.failed.append(group.path)
            _log_failure(group.path, e, ctx)
            failure = (print_failure, f"PreTest of `{group.path}` has failed.")
            section = [failure] if ctx.streaming else [header, failure]
            ctx.output.submit(group.path, section)
            if ctx.fail_fast:
                ctx.abort.set()
            return False
    return True


def _run_group_case(group: TestGroup, slot: int, ctx: RunContext):
    """
    Runs one selected case of the group into its per-slot statistics.
    Any escaping exception (e.g. an unresolvable TimeOut) fails just this
    case instead of tearing down the run.
    """
    if ctx.abort.is_set():
        return
    index, case = group.cases[slot]
    clog = group.case_logs[slot]
    try:
        if run_single_case(
            group.test, case, index, group.path, ctx, group.results[slot], clog
        ):
            ctx.abort.set()
    except Exception as e:
        case_path = f"{group.path}/{case.name}"
        message = "".join(str(arg) for arg in e.args) or repr(e)
        clog.emit(print_failure, f"Case `{case.name}` has failed: {message}")
        clog.log(f"{case_path} has failed:\n{message}\n")
        clog.flush()
        group.results[slot].failed.append(case_path)
        if ctx.fail_fast:
            ctx.abort.set()


def _finish_group(group: TestGroup, ctx: RunContext):
    """
    Merges the per-case results, runs `PostTest`, emits the group's
    output section and releases the node chain.
    """
    for result in group.results:
        group.stats.iadd(result)

    post_failure = None
    if group.test.post_test:
        log_info_if_needed("Executing post-test...", ctx.dry, ctx.verbose)
        try:
            dit_exec_command(
                group.test.post_test,
                cwd=group.test.cwd,
                capture_output=not ctx.verbose,
                dry=ctx.dry,
                verbose=ctx.verbose,
                core_dumps=ctx.core_dumps,
            )
        except Exception as e:
            group.stats.failed.append(group.path)
            _log_failure(group.path, e, ctx)
            post_failure = (print_failure, f"PostTest of `{group.path}` has failed.")
            if ctx.fail_fast:
                ctx.abort.set()

    if ctx.streaming:
        if post_failure is not None:
            ctx.output.submit(group.path, [post_failure])
    else:
        section = [(log_info, f"===== {group.path} =====")]
        for clog in group.case_logs:
            section.extend(clog.console)
        if post_failure is not None:
            section.append(post_failure)
        ctx.output.submit(group.path, section)

    _release(group.node, ctx)


def _execute_group_inline(group: TestGroup, ctx: RunContext):
    """
    Runs a whole group on the calling thread: the `--sequential` mode and
    the `NoParallel` phase, where cases run strictly one-by-one.
    """
    if ctx.clean:
        try:
            _ensure_open(group.node, ctx)
        except NodeOpenError:
            pass  # cleaning is best-effort; still try the Clean command
        clean_test(group.test, group.path, ctx)
        _release(group.node, ctx)
        return
    if _start_group(group, ctx):
        for slot in range(len(group.cases)):
            _run_group_case(group, slot, ctx)
        _finish_group(group, ctx)
    else:
        _release(group.node, ctx)


def _launch_group(group: TestGroup, ctx: RunContext, on_done):
    """
    Schedules a group on the pool: a start task opens the node and runs
    `PreTest`, then submits one task per case; the task finishing the
    last case runs `PostTest` and emits the section. A case task only
    ever waits for the threads of cases that are already running, so the
    pool cannot deadlock.
    """
    assert ctx.pool is not None
    assert ctx.threads is not None
    pool = ctx.pool
    threads = ctx.threads

    def case_task(slot: int):
        try:
            # `NeededThreads` of the case are held for its whole run, so
            # the cases running at any moment never ask the machine for
            # more than `-j` threads in total.
            if not ctx.abort.is_set():
                with ResourceAcquirer(threads, group.cases[slot][1].needed_threads):
                    _run_group_case(group, slot, ctx)
        finally:
            with group.lock:
                group.remaining -= 1
                last = group.remaining == 0
            if last:
                try:
                    _finish_group(group, ctx)
                finally:
                    on_done()

    def start_task():
        try:
            started = _start_group(group, ctx)
        except Exception as e:
            # Defensive: _start_group handles the expected failures
            # itself; anything escaping it must not hang the phase.
            group.stats.failed.append(group.path)
            _log_failure(group.path, e, ctx)
            ctx.output.emit_now([(print_failure, f"`{group.path}` has failed.")])
            ctx.output.submit(group.path, [])
            started = False
        if started:
            for slot in range(len(group.cases)):
                pool.submit(case_task, slot)
        else:
            try:
                _release(group.node, ctx)
            finally:
                on_done()

    pool.submit(start_task)


def run_groups(
    normal: list[TestGroup], deferred: list[TestGroup], ctx: RunContext
) -> TestStatistics:
    """
    Phase 1 runs the normal groups' cases concurrently on the pool;
    phase 2 then runs the `NoParallel` groups sequentially on the calling
    thread, with the machine otherwise idle.
    """
    if ctx.parallel and normal:
        done = threading.Event()
        pending = len(normal)
        pending_lock = threading.Lock()

        def on_done():
            nonlocal pending
            with pending_lock:
                pending -= 1
                if pending == 0:
                    done.set()

        for group in normal:
            _launch_group(group, ctx, on_done)
        done.wait()
    else:
        for group in normal:
            _execute_group_inline(group, ctx)

    for group in deferred:
        _execute_group_inline(group, ctx)

    stats = TestStatistics([], [], [])
    for group in normal + deferred:
        stats.iadd(group.stats)
    stats.failed += ctx.node_failures
    return stats
