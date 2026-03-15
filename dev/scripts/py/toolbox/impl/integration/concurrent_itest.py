from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
import re

from .classes import Case, Test, TestNode
from .test_loader import load_tests
from .tester import tester_impl
from .utils import dit_exec_command, print_failure, print_neutral, print_success, write_log
from ..helpers import BashCommandError, exit_with_error, get_dev_directory, log_info, log_warning


# In this test mode, we want to verify that compiling the same package with the same backend (LLVM or DVM) produces the same .o/.dbc artifacts regardless of whether duckc is run with 1 worker or 3 workers. 
# To achieve this, we need to select eligible test cases, prepare the test environment by duplicating .dmf modules, run the compile phase for both single-threaded and concurrent modes, collect artifact snapshots, and compare them for differences.
# To make dupolication works correctly, we need to do 2 things:
# 1) Rename main function in copies to avoid "Multiple main functions" error during linking stage.
# 2) Rewrite import when copying main_module file. This is because the copy of main source file
#    becomes the single-file sub_module of main module
#    When the main source file imports its child module, the copy should import 
#    the same child module but as a submodule of main module, not as a sibling module.
#    Otherwise, the copy will fail to compile due to "Module not found" error.

GENERATED_COPY_STEM_RE = re.compile(r"^concurrent_copy\d+_.+")
# we are looking for first module name in the inport statement
# e.g. in "import foo.bar.baz" we are looking for "foo" because if "foo"
# if domething changes in parser this should olso change.
IMPORT_RE = re.compile(r"^(\s*import\s+)([A-Za-z_]\w*)(?=[\s\.;]|$)")
BACKEND_CASE_RE = re.compile(r"case\s+'(llvm|dvm)'\s+in")
MAIN_FUN_RE = re.compile(r"(\bfun\s+)main(\s*\()")


@dataclass
class ConcurrentItestConfig:
    copy_count: int = 3
    blacklist: list[str] = None

    def __post_init__(self):
        if self.blacklist is None:
            self.blacklist = []


@dataclass(frozen=True)
class TestRef:
    cwd: Path
    name: str


@dataclass(frozen=True)
class CaseRef:
    test: Test
    case: Case
    path: str



def _matches_filter(path: str, filter_value: str) -> bool:
    if not filter_value:
        return True
    return path.startswith(filter_value) or filter_value.startswith(path)


def _is_blacklisted(path: str, blacklist: list[str]) -> bool:
    return any(path == item or path.startswith(item) for item in blacklist)


def _load_default_concurrent_config() -> ConcurrentItestConfig:
    default_config_path = get_dev_directory() / "integration_tests" / "concurrent_itest.yaml"
    if not default_config_path.exists():
        return ConcurrentItestConfig()

    try:
        import yaml
        with open(default_config_path) as file:
            loaded = yaml.safe_load(file.read()) or {}
    except Exception as error:
        log_warning(
            f"Failed to parse '{default_config_path}'. Using concurrent defaults. Error: {error}"
        )
        return ConcurrentItestConfig()

    blacklist = loaded.get("blacklist", [])
    if blacklist is None:
        blacklist = []
    elif not isinstance(blacklist, list) or not all(
            isinstance(item, str) for item in blacklist
    ):
        log_warning(
            f"Invalid 'blacklist' in '{default_config_path}'. Falling back to empty blacklist."
        )
        blacklist = []

    copy_count = loaded.get("copy_count", 3)
    if not isinstance(copy_count, int) or copy_count < 1:
        log_warning(
            f"Invalid 'copy_count' in '{default_config_path}'. Falling back to 3."
        )
        copy_count = 3

    return ConcurrentItestConfig(copy_count=copy_count, blacklist=blacklist)


def _detect_backend(case: Case) -> str | None:
    selector = BACKEND_CASE_RE.search(case.run)
    if selector:
        return selector.group(1)

    lowered_name = case.name.lower()
    if lowered_name in ["llvm", "dvm"]:
        return lowered_name

    return None


def _is_compile_package_case(test: Test, case: Case) -> bool:
    return "compile_package" in f"{test.pre_test}\n{case.pre_case}"


def _collect_eligible_cases(
        node: TestNode,
        tree: list[str],
        filter_value: str,
        blacklist: list[str],
        selected_case_paths: set[str],
        selected_tests: set[TestRef],
    selected_backends: dict[str, str],
):
    tree = [*tree, node.name]

    for test in node.tests:
        test_path = "/".join([*tree, test.name])
        if not _matches_filter(test_path, filter_value):
            continue

        for case in test.cases:
            case_path = f"{test_path}/{case.name}"
            if not _matches_filter(case_path, filter_value):
                continue
            if _is_blacklisted(case_path, blacklist):
                continue
            if not _is_compile_package_case(test, case):
                continue
            backend = _detect_backend(case)
            if backend not in ["llvm", "dvm"]:
                continue

            selected_case_paths.add(case_path)
            selected_tests.add(TestRef(cwd=test.cwd, name=test.name))
            selected_backends[case_path] = backend

    for subtest in node.subtests:
        _collect_eligible_cases(
            subtest,
            tree,
            filter_value,
            blacklist,
            selected_case_paths,
            selected_tests,
            selected_backends,
        )


def _is_copy_file(path: Path) -> bool:
    return path.suffix == ".dmf" and GENERATED_COPY_STEM_RE.match(path.stem) is not None


def _iter_original_dmf_files(package_dir: Path) -> list[Path]:
    return sorted(
        [
            file
            for file in package_dir.rglob("*.dmf")
            if not _is_copy_file(file)
        ]
    )


def _get_child_module_names(main_module_file: Path) -> set[str]:
    parent_dir = main_module_file.parent
    main_name = main_module_file.stem
    child_names = set()

    for child in parent_dir.iterdir():
        if child.is_file() and child.suffix == ".dmf" and not _is_copy_file(child):
            if child.stem != main_name:
                child_names.add(child.stem)
            continue

        if child.is_dir() and any(
                nested.suffix == ".dmf" and not _is_copy_file(nested)
                for nested in child.rglob("*.dmf")
        ):
            child_names.add(child.name)

    return child_names


def _rewrite_import_line_if_needed(
        line: str,
        main_module_name: str,
        child_module_names: set[str],
) -> str:
    newline = "\n" if line.endswith("\n") else ""
    line_without_newline = line[:-1] if newline else line

    match = IMPORT_RE.match(line_without_newline)
    if not match:
        return line

    first_segment = match.group(2)

    if first_segment not in child_module_names:
        return line

    if first_segment == main_module_name:
        return line

    first_segment_start, first_segment_end = match.span(2)
    rewritten = (
        f"{line_without_newline[:first_segment_start]}"
        f"{main_module_name}.{line_without_newline[first_segment_start:first_segment_end]}"
        f"{line_without_newline[first_segment_end:]}"
    )
    return f"{rewritten}{newline}"


def _rewrite_for_copy(
        source_text: str,
        source_file: Path,
        copy_index: int,
        rename_main_in_copies: bool,
) -> str:
    rewritten = source_text

    if rename_main_in_copies:
        rewritten = MAIN_FUN_RE.sub(rf"\1main_copy{copy_index}\2", rewritten)

    if source_file.stem != source_file.parent.name:
        return rewritten

    main_module_name = source_file.stem
    child_module_names = _get_child_module_names(source_file)

    rewritten_lines = [
        _rewrite_import_line_if_needed(
            line,
            main_module_name,
            child_module_names,
        )
        for line in rewritten.splitlines(keepends=True)
    ]
    return "".join(rewritten_lines)


def _index_selected_case_refs(
        node: TestNode,
        tree: list[str],
        selected_case_paths: set[str],
        indexed: dict[str, CaseRef],
):
    tree = [*tree, node.name]

    for test in node.tests:
        test_path = "/".join([*tree, test.name])
        for case in test.cases:
            case_path = f"{test_path}/{case.name}"
            if case_path in selected_case_paths:
                indexed[case_path] = CaseRef(test=test, case=case, path=case_path)

    for subtest in node.subtests:
        _index_selected_case_refs(subtest, tree, selected_case_paths, indexed)


def _run_compile_phase(case_ref: CaseRef, dry: bool, verbose: bool) -> bool:
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


def _collect_artifact_snapshot(build_dir: Path) -> dict[str, str]:
    if not build_dir.exists():
        return {}

    files = sorted(
        file
        for file in build_dir.rglob("*")
        if file.is_file() and file.suffix in {".o", ".dbc"}
    )

    snapshot: dict[str, str] = {}
    for file in files:
        rel = file.relative_to(build_dir).as_posix()
        snapshot[rel] = sha256(file.read_bytes()).hexdigest()
    return snapshot


def _diff_snapshots(single: dict[str, str], concurrent: dict[str, str]) -> list[str]:
    diffs: list[str] = []

    single_paths = set(single.keys())
    concurrent_paths = set(concurrent.keys())

    missing_in_concurrent = sorted(single_paths - concurrent_paths)
    extra_in_concurrent = sorted(concurrent_paths - single_paths)

    if missing_in_concurrent:
        diffs.append(
            "Missing in concurrent build: " + ", ".join(missing_in_concurrent[:10])
        )
    if extra_in_concurrent:
        diffs.append(
            "Extra in concurrent build: " + ", ".join(extra_in_concurrent[:10])
        )

    changed = sorted(
        path for path in (single_paths & concurrent_paths) if single[path] != concurrent[path]
    )
    if changed:
        diffs.append("Content differs: " + ", ".join(changed[:10]))

    return diffs


def _run_concurrent_compile_determinism(
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
    _index_selected_case_refs(single_thread_set, [], selected_case_paths, single_refs)
    _index_selected_case_refs(concurrent_set, [], selected_case_paths, concurrent_refs)

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
            if not _run_compile_phase(single_case, dry=dry, verbose=verbose):
                disabled.append(case_path)
                if not dry:
                    print_neutral(f"Case `{case_path}` disabled")
                continue

            single_snapshot = _collect_artifact_snapshot(single_case.test.cwd / "build")

            if not _run_compile_phase(concurrent_case, dry=dry, verbose=verbose):
                disabled.append(case_path)
                if not dry:
                    print_neutral(f"Case `{case_path}` disabled")
                continue

            concurrent_snapshot = _collect_artifact_snapshot(concurrent_case.test.cwd / "build")

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

            diffs = _diff_snapshots(single_snapshot, concurrent_snapshot)
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


def _duplicate_package_modules(
    package_dir: Path,
    copy_count: int,
    rename_main_in_copies: bool,
) -> list[Path]:
    created_files: list[Path] = []

    if not package_dir.exists():
        log_warning(f"Package dir not found: {package_dir}")
        return created_files

    originals = _iter_original_dmf_files(package_dir)
    for source_file in originals:
        source_text = source_file.read_text()
        sanitized_source_stem = source_file.stem.replace("-", "_")
        for copy_index in range(1, copy_count + 1):
            target = source_file.with_name(
                f"concurrent_copy{copy_index}_{sanitized_source_stem}.dmf"
            )
            rewritten = _rewrite_for_copy(
                source_text,
                source_file,
                copy_index,
                rename_main_in_copies,
            )
            target.write_text(rewritten)
            created_files.append(target)

    return created_files


def _remove_files(paths: list[Path]):
    for file in reversed(paths):
        try:
            if file.exists():
                file.unlink()
        except OSError as error:
            log_warning(f"Failed to remove temp file '{file}': {error}")


def concurrent_tester_impl(
        clean: bool,
        dry: bool,
        filter: str,
        fail_fast: bool,
        verbose: bool,
        log_file: str | Path,
        build_dir: str,
        duckc_worker_count: int,
):
    config = _load_default_concurrent_config()

    log_file = Path(log_file)
    if log_file.exists() and not dry:
        log_file.unlink()

    def _make_user_values(worker_count: int) -> dict[str, str]:
        return {
            "build_dir": str(Path(build_dir).absolute()),
            "dev_dir": str(get_dev_directory()),
            "duckc_worker_count": str(worker_count),
        }

    concurrent_test_set = load_tests(
        "integration_tests", user_values=_make_user_values(worker_count=3)
    )

    selected_case_paths: set[str] = set()
    selected_tests: set[TestRef] = set()
    selected_backends: dict[str, str] = {}
    _collect_eligible_cases(
        concurrent_test_set,
        [],
        filter,
        config.blacklist,
        selected_case_paths,
        selected_tests,
        selected_backends,
    )

    if not selected_case_paths:
        exit_with_error("No eligible compile_package LLVM/DVM integration test cases found.")

    llvm_cases = sum(1 for backend in selected_backends.values() if backend == "llvm")
    dvm_cases = sum(1 for backend in selected_backends.values() if backend == "dvm")
    log_info(
        "Concurrent compile determinism mode selected cases: "
        f"{len(selected_case_paths)} total ({llvm_cases} llvm, {dvm_cases} dvm)."
    )

    if duckc_worker_count != 3:
        log_info(
            "Concurrent mode overrides duckc worker count to 3. "
            f"Ignoring provided value: {duckc_worker_count}."
        )

    if clean:
        tester_impl(
            clean=clean,
            dry=dry,
            filter=filter,
            fail_fast=fail_fast,
            verbose=verbose,
            log_file=log_file,
            build_dir=build_dir,
            duckc_worker_count=3,
            allowed_case_paths=selected_case_paths,
        )
        return

    created_files: list[Path] = []
    if not dry:
        log_info("Preparing temporary duplicated .dmf modules for concurrent itests...")
        for test_ref in sorted(selected_tests, key=lambda ref: str(ref.cwd / ref.name)):
            package_dir = test_ref.cwd / "duck_modules" / test_ref.name
            created_files += _duplicate_package_modules(
                package_dir,
                config.copy_count,
                True,
            )

    try:
        single_thread_set = load_tests(
            "integration_tests", user_values=_make_user_values(worker_count=1)
        )
        concurrent_set = load_tests(
            "integration_tests", user_values=_make_user_values(worker_count=3)
        )

        succeeded, failed, disabled = _run_concurrent_compile_determinism(
            selected_case_paths=selected_case_paths,
            single_thread_set=single_thread_set,
            concurrent_set=concurrent_set,
            dry=dry,
            fail_fast=fail_fast,
            verbose=verbose,
            log_file=log_file,
        )

        if dry:
            return

        total_test_count = len(succeeded) + len(failed) + len(disabled)
        print(f"Ran test count: {total_test_count}")
        print(f" - Succeeded: {len(succeeded)}")
        print(f" - Disabled:  {len(disabled)}")
        print(f" - Failed:    {len(failed)}")

        if failed:
            failed_tests = map(lambda x: " - " + x, failed)
            exit_with_error(
                f"{'(Fail fast) ' if fail_fast else ''}Failed tests:\n{chr(10).join(failed_tests)}\n"
                + f"Please see log file '{log_file.absolute()}' for more info."
            )

        print_success("All concurrent compile determinism tests have run successfully!")
    finally:
        if created_files:
            log_info("Cleaning up temporary duplicated .dmf modules...")
            _remove_files(created_files)
