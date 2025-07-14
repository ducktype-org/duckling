#!/usr/bin/env python3

import pathlib
import sys

import click


from scripts.py.toolbox.helpers import exit_with_error

from scripts.py.toolbox.integration.tester import itest
from scripts.py.toolbox.clean_init import clean_init
from scripts.py.toolbox.coverage import coverage
from scripts.py.toolbox.cpp_linter import cpp_linter
from scripts.py.toolbox.docs import docs
from scripts.py.toolbox.download_binaries import download_binaries
from scripts.py.toolbox.download_llvm import download_llvm
from scripts.py.toolbox.duck_linter import duck_linter
from scripts.py.toolbox.init import init
from scripts.py.toolbox.install_llvm import install_llvm
from scripts.py.toolbox.issue_checker import issue_checker
from scripts.py.toolbox.pr_validate import pr_validate
from scripts.py.toolbox.run_cpp_preprocessor import run_preprocessor
from scripts.py.toolbox.setup_build import setup_build
from scripts.py.toolbox.setup_venv import setup_venv
from scripts.py.toolbox.test import test
from scripts.py.toolbox.todo_counter import todo_counter


DATA_USER = "dev"
# @FUTURE: change this password and hide it:
DATA_PASS = "7ocwXWOAwg="


@click.group()
def cli():
    pass


cli.add_command(itest)

cli.add_command(clean_init)

cli.add_command(coverage)

cli.add_command(cpp_linter)

cli.add_command(docs)

cli.add_command(download_binaries)

cli.add_command(download_llvm)

cli.add_command(duck_linter)

cli.add_command(init)

cli.add_command(install_llvm)

cli.add_command(issue_checker)

cli.add_command(pr_validate)

cli.add_command(run_preprocessor)

cli.add_command(setup_build)

cli.add_command(setup_venv)

cli.add_command(test)

cli.add_command(todo_counter)


if __name__ == "__main__":
    if pathlib.Path.cwd() != pathlib.Path(__file__).parent.absolute():
        exit_with_error("Toolbox should be called from the root of the project")

    # Disable traceback for shorter error messages.
    # Comment this line when debugging.
    sys.tracebacklimit = 0

    cli()
