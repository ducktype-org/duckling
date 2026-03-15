from dataclasses import dataclass
from pathlib import Path
import re

from ..classes import Case, Test, TestNode


BACKEND_CASE_RE = re.compile(r"case\s+'(llvm|dvm)'\s+in")


@dataclass(frozen=True)
class TestRef:
    cwd: Path
    name: str


@dataclass(frozen=True)
class CaseRef:
    test: Test
    case: Case
    path: str


def matches_filter(path: str, filter_value: str) -> bool:
    if not filter_value:
        return True
    return path.startswith(filter_value) or filter_value.startswith(path)


def is_blacklisted(path: str, blacklist: list[str]) -> bool:
    return any(path == item or path.startswith(item) for item in blacklist)


def detect_backend(case: Case) -> str | None:
    selector = BACKEND_CASE_RE.search(case.run)
    if selector:
        return selector.group(1)

    lowered_name = case.name.lower()
    if lowered_name in ["llvm", "dvm"]:
        return lowered_name

    return None


def is_compile_package_case(test: Test, case: Case) -> bool:
    return "compile_package" in f"{test.pre_test}\n{case.pre_case}"


def collect_eligible_cases(
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
        if not matches_filter(test_path, filter_value):
            continue

        for case in test.cases:
            case_path = f"{test_path}/{case.name}"
            if not matches_filter(case_path, filter_value):
                continue
            if is_blacklisted(case_path, blacklist):
                continue
            if not is_compile_package_case(test, case):
                continue
            backend = detect_backend(case)
            if backend not in ["llvm", "dvm"]:
                continue

            selected_case_paths.add(case_path)
            selected_tests.add(TestRef(cwd=test.cwd, name=test.name))
            selected_backends[case_path] = backend

    for subtest in node.subtests:
        collect_eligible_cases(
            subtest,
            tree,
            filter_value,
            blacklist,
            selected_case_paths,
            selected_tests,
            selected_backends,
        )


def index_selected_case_refs(
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
        index_selected_case_refs(subtest, tree, selected_case_paths, indexed)
