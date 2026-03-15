from pathlib import Path

from ..classes import TestNode
from ..utils import dit_exec_command, print_failure, print_neutral, print_success, write_log
from ...helpers import BashCommandError, log_info
from .artifact_diff import collect_artifact_snapshot, diff_snapshots
from .selection import CaseRef, index_selected_case_refs


def run_compile_phase(case_ref: CaseRef, dry: bool, verbose: bool) -> bool:
    test = case_ref.test
    case = case_ref.case

    if case.enabled != "":
        try:
            dit_exec_command(
                case.enabled,
                cwd=test.cwd,
                capture_output=not verbose,
                dry=dry,
                verbose=verbose,
                exitcode=0,
            )
        except BashCommandError:
            return False

    if case.pre_case:
        dit_exec_command(
            case.pre_case,
            cwd=test.cwd,
            capture_output=not verbose,
            dry=dry,
            verbose=verbose,
        )

    return True


def run_concurrent_compile_determinism(
    selected_case_paths: set[str],
    single_thread_set: TestNode,
    concurrent_set: TestNode,
    dry: bool,
    fail_fast: bool,
    verbose: bool,
    log_file: Path,
) -> tuple[list[str], list[str], list[str]]:
    single_refs: dict[str, CaseRef] = {}
    concurrent_refs: dict[str, CaseRef] = {}
    index_selected_case_refs(single_thread_set, [], selected_case_paths, single_refs)
    index_selected_case_refs(concurrent_set, [], selected_case_paths, concurrent_refs)

    succeeded: list[str] = []
    failed: list[str] = []
    disabled: list[str] = []

    for case_path in sorted(selected_case_paths):
        single_case = single_refs.get(case_path)
        concurrent_case = concurrent_refs.get(case_path)
        if single_case is None or concurrent_case is None:
            failed.append(case_path)
            message = f"Case index mismatch for `{case_path}` between single and concurrent test trees."
            print_failure(message)
            write_log(message + "\n", log_file)
            if fail_fast:
                break
            continue

        log_info(f"===== {case_path} =====")

        try:
            if not run_compile_phase(single_case, dry=dry, verbose=verbose):
                disabled.append(case_path)
                if not dry:
                    print_neutral(f"Case `{case_path}` disabled")
                continue

            single_snapshot = collect_artifact_snapshot(single_case.test.cwd / "build")

            if not run_compile_phase(concurrent_case, dry=dry, verbose=verbose):
                disabled.append(case_path)
                if not dry:
                    print_neutral(f"Case `{case_path}` disabled")
                continue

            concurrent_snapshot = collect_artifact_snapshot(concurrent_case.test.cwd / "build")

            if dry:
                continue

            if not single_snapshot and not concurrent_snapshot:
                failed.append(case_path)
                message = (
                    f"No .o/.dbc artifacts found for `{case_path}` after single-thread and concurrent builds."
                )
                print_failure(message)
                write_log(message + "\n", log_file)
                if fail_fast:
                    break
                continue

            diffs = diff_snapshots(single_snapshot, concurrent_snapshot)
            if diffs:
                failed.append(case_path)
                reason = f"Artifacts differ for `{case_path}`: {' | '.join(diffs)}"
                print_failure(reason)
                write_log(reason + "\n", log_file)
                if fail_fast:
                    break
                continue

            succeeded.append(case_path)
            print_success(f"Case `{case_path}` passed (artifacts identical)")

        except BashCommandError as error:
            failed.append(case_path)
            print_failure(f"Case `{case_path}` has failed during compilation.")
            write_log(
                f"{case_path} has failed during compilation:\n{''.join(error.args)}\n",
                log_file=log_file,
            )
            if fail_fast:
                break

    return succeeded, failed, disabled
