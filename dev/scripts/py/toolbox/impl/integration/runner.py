import os
from pathlib import Path

from .classes import Case, Test
from .context import RunContext
from .reporting import (
    CaseLog,
    Disabled,
    OutputMismatch,
    Success,
    TestStatistics,
    print_failure,
    print_neutral,
    print_success,
)
from .utils import dit_exec_command
from ..helpers import BashCommandError, log_info, log_warning


def resolve_timeout(timeout, cwd: Path) -> float:
    """
    Resolves a TimeOut value to seconds. It may be a number or a bash
    expression producing one (e.g. inspecting the build configuration).
    """
    try:
        return float(timeout)
    except ValueError:
        pass
    value, _ = dit_exec_command(f"echo -n {timeout}", cwd=cwd)
    try:
        return float(value.decode("UTF-8").strip())
    except ValueError:
        # Raise instead of exiting: this runs on a worker thread, where
        # an exit would kill only the thread and go unnoticed.
        raise RuntimeError(f"TimeOut `{timeout}` does not evaluate to a number.")


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
        match run_case(test, case, case_path, ctx, clog):
            case OutputMismatch(error):
                clog.emit(
                    print_failure, f"Case `{case.name}` has failed because {error}"
                )
                stats.failed.append(case_path)
                stop = ctx.fail_fast
            case Success():
                if not ctx.dry:
                    stats.succeeded.append(case_path)
                    clog.emit(print_success, f"Case `{case.name}` passed")
            case Disabled():
                if not ctx.dry:
                    stats.disabled.append(case_path)
                    clog.emit(print_neutral, f"Case `{case.name}` disabled")
    except BashCommandError as e:
        clog.emit(print_failure, f"Case `{test.name}/{case.name}` has {e.reason_string}.")
        clog.log(f"{case_path} has failed:\n{''.join(e.args)}\n")
        stats.failed.append(case_path)
        stop = ctx.fail_fast
    clog.flush()
    return stop


def log_test_out_differs(case_path, message, got, expected, clog: CaseLog, verbose):
    """
    Used to dump program's incorrect output.
    """
    to_dump = (
        f"{case_path}: {message}\n"
        f"[GOT]:\n{got.decode('UTF-8')}\n"
        f"[EXPECTED]:\n{expected.decode('UTF-8')}\n"
    )
    if verbose:
        clog.emit(log_info, to_dump)
    clog.log(to_dump)


def run_case(
    test: Test, case: Case, case_path: str, ctx: RunContext, clog: CaseLog
) -> Success | OutputMismatch | Disabled:
    """
    Runs a test case from `Case` object.
    It may emit `BashCommandError`s.
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
                core_dumps=ctx.core_dumps,
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
                command,
                cwd=test.cwd,
                dry=dry,
                verbose=verbose,
                env=case_env,
                core_dumps=ctx.core_dumps,
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
            core_dumps=ctx.core_dumps,
        )

    # Get input.
    test_input = bytes()
    if case.input:
        clog.info_if_needed("Getting input", dry, verbose)
        test_input, _ = dit_exec_command(
            case.input.get_command(),
            cwd=test.cwd,
            dry=dry,
            verbose=verbose,
            env=case_env,
            core_dumps=ctx.core_dumps,
        )

    # Run test.
    clog.info_if_needed("Running the test case...", dry, verbose)
    test_output, test_err = dit_exec_command(
        case.run,
        cwd=test.cwd,
        input=test_input,
        exitcode=case.expected_exitcode,
        dry=dry,
        verbose=verbose,
        env=case_env,
        timeout=(
            None
            if dry
            else ctx.timeout_scale * resolve_timeout(case.timeout, test.cwd)
        ),
        core_dumps=ctx.core_dumps,
    )

    # Compare test and expected output.
    if case.expected_output:
        clog.info_if_needed("Getting the expected output...", dry, verbose)
        test_expected_output, _ = dit_exec_command(
            case.expected_output.get_command(),
            cwd=test.cwd,
            verbose=verbose,
            dry=dry,
            env=case_env,
            core_dumps=ctx.core_dumps,
        )
        if not dry and test_output != test_expected_output:
            log_test_out_differs(
                case_path,
                "Stdouts do not match.",
                test_output,
                test_expected_output,
                clog,
                verbose,
            )
            return OutputMismatch(f"Stdouts do not match.")

    # Compare test and expected err.
    if case.expected_err:
        clog.info_if_needed("Getting the expected err...", dry, verbose)
        test_expected_err, _ = dit_exec_command(
            case.expected_err.get_command(),
            cwd=test.cwd,
            verbose=verbose,
            dry=dry,
            env=case_env,
            core_dumps=ctx.core_dumps,
        )
        if not dry and test_err != test_expected_err:
            log_test_out_differs(
                case_path,
                "Stderrs do not match.",
                test_err,
                test_expected_err,
                clog,
                verbose,
            )
            return OutputMismatch(f"Stderrs do not match.")

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
            core_dumps=ctx.core_dumps,
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
                core_dumps=ctx.core_dumps,
            )
    except BashCommandError:
        log_warning(f"Cleaning has (partially) failed on {test.name}.")
