import json
import os
import threading
from dataclasses import dataclass
from pathlib import Path

from .test_loader import Case, Test, TestNode, load_tests
from .utils import (
    CaseLog,
    OrderedOutput,
    dit_exec_command,
    log_info_if_needed,
    print_failure,
    print_success,
    write_log,
    print_neutral,
    TestStatistics,
    Success,
    Failure,
    Disabled,
)
from ..helpers import (
    BashCommandError,
    exit_with_error,
    get_input,
    log_info,
    log_warning,
    get_dev_directory,
)

DEFAULT_LOG_FILE_PATH = Path("/tmp/dit.log")

TIMEOUT_CACHE: dict[str, str] = {}
TIMEOUT_CACHE_LOCK = threading.Lock()


def eval_timeout(timeout, cwd: Path, dry: bool) -> str:
    """
    Resolves a TimeOut expression to its value. The expression may invoke
    commands (e.g. inspecting the build configuration), so it is evaluated
    once per unique expression and cached; this also keeps the logged Run
    commands readable.
    """
    timeout = str(timeout)
    if dry:
        return timeout
    with TIMEOUT_CACHE_LOCK:
        if timeout in TIMEOUT_CACHE:
            return TIMEOUT_CACHE[timeout]
    value, _ = dit_exec_command(f"echo -n {timeout}", cwd=cwd)
    value = value.decode("UTF-8").strip() or timeout
    with TIMEOUT_CACHE_LOCK:
        TIMEOUT_CACHE[timeout] = value
    return value


@dataclass
class RunContext:
    """
    Options and shared state of a single `itest` invocation.
    """

    filter: str
    clean: bool
    dry: bool
    fail_fast: bool
    verbose: bool
    log_file: Path
    jobs: int
    case_slots: threading.BoundedSemaphore
    log_lock: threading.Lock
    abort: threading.Event
    output: OrderedOutput

    @property
    def parallel(self) -> bool:
        return self.jobs > 1


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
        jobs: int = 1,
):
    """
    The driver function of Duckling Integration Tests framework.
    """
    log_info("Running integration tests...")

    log_file = Path(log_file)
    if log_file.exists():
        if log_file.absolute() != DEFAULT_LOG_FILE_PATH and log_file.stat().st_size > 0:
            log_warning(
                f"File: {log_file} already exists and is NOT empty! It will be overwritten!"
            )
            inp = get_input(f"Continue anyway: [y/N] ").lower()
            if inp not in ["y"]:
                exit_with_error("Exiting...")
        log_file.unlink()
        log_file = Path(log_file)

    user_values = json.loads(custom_values)

    user_values["build_dir"] = str(Path(build_dir).absolute())
    user_values["dev_dir"] = str(get_dev_directory())
    if determinism_check is not None:
        user_values["enable_conc_deterministic_tests"] = str(determinism_check).lower()

    test_set = load_tests("integration_tests", user_values=user_values)

    # `--dry` and `--clean` keep the deterministic, sequential order.
    if dry or clean:
        jobs = 1
    jobs = max(1, jobs)

    ctx = RunContext(
        filter=filter,
        clean=clean,
        dry=dry,
        fail_fast=fail_fast,
        verbose=verbose,
        log_file=log_file,
        jobs=jobs,
        case_slots=threading.BoundedSemaphore(jobs),
        log_lock=threading.Lock(),
        abort=threading.Event(),
        output=OrderedOutput(),
    )

    if ctx.parallel:
        register_test_sections(test_set, [], ctx)

    (succeeded, failed, disabled) = run_tests(test_set, [], ctx)
    ctx.output.drain()

    if dry:
        return

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


def register_test_sections(node: TestNode, tree: list[str], ctx: RunContext):
    """
    Registers every test that will run in tree order, so that the output
    of concurrently running tests is printed in that order. Mirrors the
    traversal and filtering of `run_tests`.
    """
    tree.append(node.name)
    current_path = "/".join(tree)
    if not current_path.startswith(ctx.filter) and not ctx.filter.startswith(current_path):
        return
    for test in node.tests:
        path = "/".join(tree + [test.name])
        if not path.startswith(ctx.filter) and not ctx.filter.startswith(path):
            continue
        ctx.output.register(path)
    for subtest in node.subtests:
        register_test_sections(subtest, tree.copy(), ctx)


def run_test(test: Test, path: str, ctx: RunContext) -> TestStatistics:
    """
    Runs a test from `Test` object.
    `PreTest` runs before and `PostTest` after all of the test's cases;
    with `jobs > 1` the cases themselves run concurrently.
    """
    if test.pre_test:
        log_info_if_needed("Executing pre-test...", ctx.dry, ctx.verbose)
        dit_exec_command(
            test.pre_test,
            cwd=test.cwd,
            capture_output=not ctx.verbose,
            dry=ctx.dry,
            verbose=ctx.verbose,
        )
    stats = TestStatistics([], [], [])
    simplified_filter = ctx.filter[len(path) + 1:]

    selected = [
        (i, case)
        for i, case in enumerate(test.cases)
        if case.name.startswith(simplified_filter)
    ]

    if not ctx.parallel:
        log_info(f"===== {path} =====")
        for i, case in selected:
            clog = CaseLog(ctx.log_file)
            if run_single_case(test, case, i, path, ctx, stats, clog):
                break
    else:
        results = [TestStatistics([], [], []) for _ in selected]
        case_logs = [CaseLog(ctx.log_file, immediate=False) for _ in selected]

        def case_worker(index: int, case: Case, case_stats: TestStatistics, clog: CaseLog):
            if ctx.abort.is_set():
                return
            with ctx.case_slots:
                if ctx.abort.is_set():
                    return
                if run_single_case(test, case, index, path, ctx, case_stats, clog):
                    ctx.abort.set()

        threads = [
            threading.Thread(
                target=case_worker, args=(i, case, results[slot], case_logs[slot])
            )
            for slot, (i, case) in enumerate(selected)
        ]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        for result in results:
            stats.iadd(result)

        # Emit the whole test as one section, cases in definition order.
        section = [(log_info, f"===== {path} =====")]
        for clog in case_logs:
            section.extend(clog.console)
        ctx.output.submit(path, section)

    if test.post_test:
        log_info_if_needed("Executing post-test...", ctx.dry, ctx.verbose)
        dit_exec_command(
            test.post_test,
            cwd=test.cwd,
            capture_output=not ctx.verbose,
            dry=ctx.dry,
            verbose=ctx.verbose,
        )
    return stats


def run_single_case(
        test: Test,
        case: Case,
        index: int,
        path: str,
        ctx: RunContext,
        stats: TestStatistics,
        clog: CaseLog,
) -> bool:
    """
    Runs one case, classifies the result into `stats` and flushes the
    case's log. Returns whether a fail-fast run should stop.
    """
    clog.info_if_needed(
        f"Run [{index + 1}/{len(test)}] - {case.name}", ctx.dry, ctx.verbose
    )
    case_path = path + "/" + case.name
    stop = False
    try:
        match run_case(test, case, ctx, clog):
            case Failure(error):
                clog.emit(
                    print_failure, f"Case `{case.name}` has failed because: {error}"
                )
                stats.failed.append(case_path)
            case Success():
                if not ctx.dry:
                    stats.succeeded.append(case_path)
                    clog.emit(print_success, f"Case `{case.name}` passed")
            case Disabled():
                if not ctx.dry:
                    stats.disabled.append(case_path)
                    clog.emit(print_neutral, f"Case `{case.name}` disabled")
    except BashCommandError as e:
        if e.exit_code == 124:
            clog.emit(
                print_failure,
                f"Case `{test.name}/{case.name}` has failed with exit code 124 - likely timed out after {e.timeout} second(s).",
            )
        else:
            clog.emit(print_failure, f"Case `{test.name}/{case.name}` has failed.")
        clog.log(f"{test.name}/{case.name} has failed:\n{''.join(e.args)}\n")
        stats.failed.append(case_path)
        stop = ctx.fail_fast
    clog.flush()
    return stop


def log_test_out_differs(test, case, message, got, expected, clog: CaseLog, verbose):
    """
    Used to dump program's incorrect output.
    """
    to_dump = (
        f"{test.name}/{case.name}: {message}\n"
        f"[GOT]:\n{got.decode('UTF-8')}\n"
        f"[EXPECTED]:\n{expected.decode('UTF-8')}\n"
    )
    if verbose:
        clog.emit(log_info, to_dump)
    clog.log(to_dump)


def run_case(
        test: Test, case: Case, ctx: RunContext, clog: CaseLog
) -> Success | Failure | Disabled:
    """
    Runs a test case from `Case` object.
    Returns an empty string on success, and an error message on error.
    """
    dry = ctx.dry
    verbose = ctx.verbose

    # Check if `Enabled` evaluates to `true` to see if test-case is enabled or not
    if case.enabled != "":
        clog.info_if_needed("Checking if test-case is enabled...", dry, verbose)
        try:
            dit_exec_command(
                case.enabled,
                cwd=test.cwd,
                capture_output=not verbose,
                dry=dry,
                verbose=verbose,
                exitcode=0,  # expects 0 -- 0 means it has evaluated to `true`
            )
        except BashCommandError:
            return Disabled()

    # Evaluate `Env` variables. Each entry is a bash command whose stdout
    # becomes the variable's value; later entries see the earlier ones.
    # The resulting environment is passed to every command of this case.
    case_env = None
    if case.env:
        clog.info_if_needed("Evaluating Env variables...", dry, verbose)
        case_env = os.environ.copy()
        for name, command in case.env.items():
            value, _ = dit_exec_command(
                command, cwd=test.cwd, dry=dry, verbose=verbose, env=case_env
            )
            case_env[name] = value.decode("UTF-8").strip()

    # Pre-case command
    if case.pre_case:
        clog.info_if_needed("Executing pre-case command...", dry, verbose)
        dit_exec_command(
            case.pre_case,
            cwd=test.cwd,
            capture_output=not verbose,
            dry=dry,
            verbose=verbose,
            env=case_env,
        )

    # Get input.
    test_input = bytes()
    if case.input:
        clog.info_if_needed("Getting input", dry, verbose)
        test_input, _ = dit_exec_command(
            case.input.get_command(), cwd=test.cwd, dry=dry, verbose=verbose,
            env=case_env,
        )

    # Run test.
    clog.info_if_needed("Running the test case...", dry, verbose)
    timeout = eval_timeout(case.timeout, test.cwd, dry)
    try:
        test_output, test_err = dit_exec_command(
            f"timeout {timeout}s bash -c \'{case.run}\'",
            cwd=test.cwd,
            input=test_input,
            exitcode=case.expected_exitcode,
            dry=dry,
            verbose=verbose,
            env=case_env,
        )
    except BashCommandError as e:
        e.timeout = timeout
        raise

    # Compare test and expected output.
    if case.expected_output:
        clog.info_if_needed("Getting the expected output...", dry, verbose)
        test_expected_output, _ = dit_exec_command(
            case.expected_output.get_command(), cwd=test.cwd, verbose=verbose, dry=dry,
            env=case_env,
        )
        if not dry and test_output != test_expected_output:
            log_test_out_differs(
                test,
                case,
                "Stdouts do not match.",
                test_output,
                test_expected_output,
                clog,
                verbose,
            )
            return Failure(f"Stdouts do not match.")

    # Compare test and expected err.
    if case.expected_err:
        clog.info_if_needed("Getting the expected err...", dry, verbose)
        test_expected_err, _ = dit_exec_command(
            case.expected_err.get_command(), cwd=test.cwd, verbose=verbose, dry=dry,
            env=case_env,
        )
        if not dry and test_err != test_expected_err:
            log_test_out_differs(
                test,
                case,
                "Stderrs do not match.",
                test_err,
                test_expected_err,
                clog,
                verbose,
            )
            return Failure(f"Stderrs do not match.")

    # Post-case command
    if case.post_case:
        clog.info_if_needed("Executing post-case command...", dry, verbose)
        dit_exec_command(
            case.post_case,
            test.cwd,
            capture_output=not verbose,
            input=test_output,
            verbose=verbose,
            dry=dry,
            env=case_env,
        )

    return Success()


def clean_test(test: Test, path: str, ctx: RunContext):
    """
    Performs cleaning on a test.
    """
    log_info(f"Cleaning: {path}")
    try:
        if test.clean:
            dit_exec_command(
                test.clean,
                cwd=test.cwd,
                capture_output=False,
                dry=ctx.dry,
                verbose=ctx.verbose,
            )
    except BashCommandError:
        log_warning(f"Cleaning has (partially) failed on {test.name}.")


def run_tests(node: TestNode, tree: list[str], ctx: RunContext) -> TestStatistics:
    """
    A recursive function for running all tests.
    A single call executes all tests in a given tree node.

    Each tree node is a `testconfig.yaml` file.
    `tree` variable is used to store the names of the nodes on the
    path to the current node in the tree.

    `PreNode` runs before, and `PostNode` after, all tests and subnodes
    of the node; with `jobs > 1` the tests and subnodes in between run
    concurrently with each other.
    """
    tree.append(node.name)
    all_stats = TestStatistics([], [], [])

    # Check if the current node is relevant to the filter
    current_path = "/".join(tree)
    if not current_path.startswith(ctx.filter) and not ctx.filter.startswith(current_path):
        return all_stats

    if ctx.abort.is_set():
        return all_stats

    # Pre-node command
    if node.pre_node:
        log_info_if_needed("Executing pre-node command...", ctx.dry, ctx.verbose)
        dit_exec_command(
            node.pre_node,
            cwd=node.cwd,
            capture_output=not ctx.verbose,
            dry=ctx.dry,
            verbose=ctx.verbose,
        )

    if not ctx.parallel:
        for test in node.tests:
            path = "/".join(tree + [test.name])

            # This is tricky, as normally it would be enough to check for the prefix
            # like `pth.startswith(test_path)`, but names of test **cases** are not included
            # in the tree list of nodes, but can be in `test_path`, so we have to this both ways.
            if not path.startswith(ctx.filter) and not ctx.filter.startswith(path):
                continue

            if ctx.clean:
                clean_test(test, path, ctx)
            else:
                stats = run_test(test, path, ctx)
                all_stats += stats
                if ctx.fail_fast and len(stats.failed) > 0:
                    return all_stats

        for subtest in node.subtests:
            all_stats += run_tests(subtest, tree.copy(), ctx)
    else:
        tasks = []
        for test in node.tests:
            path = "/".join(tree + [test.name])
            if not path.startswith(ctx.filter) and not ctx.filter.startswith(path):
                continue
            tasks.append((path, lambda test=test, path=path: run_test(test, path, ctx)))
        for subtest in node.subtests:
            path = "/".join(tree + [subtest.name])
            tasks.append(
                (path, lambda subtest=subtest: run_tests(subtest, tree.copy(), ctx))
            )

        results: list[TestStatistics | None] = [None] * len(tasks)

        def node_worker(index: int, path: str, task):
            try:
                results[index] = task()
            except BashCommandError as e:
                failure = (print_failure, f"`{path}` has failed.")
                if ctx.output.has(path):
                    ctx.output.submit(path, [failure])
                else:
                    with ctx.log_lock:
                        failure[0](failure[1])
                write_log(f"{path} has failed:\n{''.join(e.args)}\n", ctx.log_file)
                results[index] = TestStatistics([], [path], [])
                if ctx.fail_fast:
                    ctx.abort.set()

        threads = [
            threading.Thread(target=node_worker, args=(i, path, task))
            for i, (path, task) in enumerate(tasks)
        ]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        for result in results:
            if result is not None:
                all_stats += result

    # Post-node command
    if node.post_node:
        log_info_if_needed("Executing post-node command...", ctx.dry, ctx.verbose)
        dit_exec_command(
            node.post_node,
            cwd=node.cwd,
            capture_output=not ctx.verbose,
            dry=ctx.dry,
            verbose=ctx.verbose,
        )

    return all_stats
