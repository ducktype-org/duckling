from .classes import Case, Test, TestNode
from ..helpers import exit_with_error
from .utils import ExpressionFillError, VariableNotFound, check_resembles_builtin
from .io_data import make_data_from_dict
from .config import (
    GENERAL_VARIABLES,
    config_find_and_eval,
    config_find_value,
    config_get_name_path,
    load_config,
)
from .keys import *


"""
Builtin keys allowed inside a Test.
"""
TEST_ALLOWED_KEYS = {*GENERAL_VARIABLES, NAME, DESCRIPTION, CASES, PARENT}


"""
Builtin keys allowed inside a Case.
"""
CASE_ALLOWED_KEYS = {NAME, INPUT, OUTPUT, ERR, RUN_ARGS, TIME_OUT, EXIT_CODE, PARENT}


def _make_case(test_dict: dict, case_name: str) -> Case:
    """
    Creates a `Case` object from a test_dict.
    """

    case_dict = test_dict[CASES][case_name]

    for var in case_dict:
        check_resembles_builtin(var, CASE_ALLOWED_KEYS)

    io_data = [None, None, None]
    config_dir = test_dict[PARENT][CONFIG_FILE].parent
    for i, io in enumerate([INPUT, OUTPUT, ERR]):
        if io in case_dict:
            io_data[i] = make_data_from_dict(case_dict[io], config_dir)

    return Case(
        name=case_dict.get(NAME, case_name),
        run_args=case_dict.get(RUN_ARGS, ""),
        input=io_data[0],
        expected_output=io_data[1],
        expected_err=io_data[2],
        expected_exitcode=config_find_value(case_dict, EXIT_CODE, default=0),
        timeout=config_find_value(case_dict, TIME_OUT, default=1),
    )


def _make_test(config: dict, test_name) -> Test:
    """
    Creates a `Test` object from a config.
    """
    test_dict = config[TESTS][test_name]
    test_path = config_get_name_path(test_dict)

    for var in test_dict:
        check_resembles_builtin(var, TEST_ALLOWED_KEYS)

    if not CASES in test_dict or not test_dict[CASES]:
        exit_with_error(f"Test {test_path} has no Cases.")

    try:
        return Test(
            name=test_dict.get(NAME, test_name),
            description=test_dict.get(DESCRIPTION),
            compile=config_find_and_eval(test_dict, COMPILE),
            run=config_find_and_eval(test_dict, RUN),
            post_run=config_find_and_eval(test_dict, POST_RUN),
            cwd=config[CONFIG_FILE].parent,
            cases=[_make_case(test_dict, case) for case in test_dict[CASES]],
            clean=config_find_and_eval(test_dict, CLEAN),
            fail_fast=config_find_value(test_dict, FAIL_FAST, default=False),
        )
    except (VariableNotFound, ExpressionFillError) as e:
        exit_with_error(
            f"In test case {test_path}\n\t{e.__class__.__name__}: {''.join(e.args)}"
        )


def _make_test_node(config: dict) -> TestNode:
    """
    Creates a `TestNode` object from a config.
    """
    return TestNode(
        name=config[NAME],
        subtests=[_make_test_node(subdir) for subdir in config.get(SUBDIRS, [])],
        tests=[_make_test(config, test) for test in config.get(TESTS, [])],
    )


def load_tests(config_dir: str) -> TestNode:
    """
    Loads tests from `config_dir` directory.
    """
    config = load_config(config_dir)
    return _make_test_node(config)
