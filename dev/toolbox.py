#!/usr/bin/env python3

import pathlib
import sys

import click

from scripts.py.toolbox.impl.helpers import exit_with_error

from scripts.py.toolbox.commands.clean_init import clean_init
from scripts.py.toolbox.commands.coverage import coverage
from scripts.py.toolbox.commands.cpp_linter import cpp_linter
from scripts.py.toolbox.commands.docs import docs
from scripts.py.toolbox.commands.download_binaries import download_binaries
from scripts.py.toolbox.commands.download_llvm import download_llvm
from scripts.py.toolbox.commands.duck_linter import duck_linter
from scripts.py.toolbox.commands.fix_coverage import fix_coverage
from scripts.py.toolbox.commands.init import init
from scripts.py.toolbox.commands.install_llvm import install_llvm
from scripts.py.toolbox.commands.issue_checker import issue_checker
from scripts.py.toolbox.commands.itest import itest
from scripts.py.toolbox.commands.list_files import list_files
from scripts.py.toolbox.commands.pr_validate import pr_validate
from scripts.py.toolbox.commands.run_preprocessor import run_preprocessor
from scripts.py.toolbox.commands.setup_build import setup_build
from scripts.py.toolbox.commands.setup_venv import setup_venv
from scripts.py.toolbox.commands.test import test
from scripts.py.toolbox.commands.todo_counter import todo_counter
from scripts.py.toolbox.commands.todo_validate import todo_validate


DATA_USER = "dev"
# @FUTURE: change this password and hide it:
DATA_PASS = "7ocwXWOAwg="


@click.group()
def cli():
    pass


cli.add_command(clean_init)
cli.add_command(coverage)
cli.add_command(cpp_linter)
cli.add_command(docs)
cli.add_command(download_binaries)
cli.add_command(download_llvm)
cli.add_command(duck_linter)
cli.add_command(fix_coverage)
cli.add_command(init)
cli.add_command(install_llvm)
cli.add_command(issue_checker)
cli.add_command(itest)
cli.add_command(list_files)
cli.add_command(pr_validate)
cli.add_command(run_preprocessor)
cli.add_command(setup_build)
cli.add_command(setup_venv)
cli.add_command(test)
cli.add_command(todo_counter)
cli.add_command(todo_validate)


if __name__ == "__main__":
    if pathlib.Path.cwd() != pathlib.Path(__file__).parent.absolute():
        exit_with_error("Toolbox should be called from the root of the project")

    # Disable traceback for shorter error messages.
    # Comment this line when debugging.
    sys.tracebacklimit = 0

    cli(max_content_width=120)
