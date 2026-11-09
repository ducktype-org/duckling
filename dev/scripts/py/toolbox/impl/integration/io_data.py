# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import abc
from pathlib import Path

from ..helpers import bash_command, truncate_str
from .keys import *


class IOData(abc.ABC):
    """
    This object represents test's IO, like input, expected output, expected err.
    It is a abstract base class and is not meant to be used directly.
    """

    @abc.abstractmethod
    def get_command(self) -> str:
        """A shell command to write the data to stdout"""

    @abc.abstractmethod
    def __str__(self) -> str:
        """Type of the data"""


class IODataFromFile(IOData):
    """
    Test's IO from File.
    """

    def __init__(self, file: Path):
        self.file = file

    def get_command(self) -> str:
        return f"cat {self.file}"

    def __str__(self) -> str:
        return f"File: {self.file}"


class IODataFromString(IOData):
    """
    Test's IO in a form of a string literal.
    """

    def __init__(self, string: str):
        self.string = string

    def get_command(self) -> str:
        return f'echo -ne "{self.string}"'

    def __str__(self) -> str:
        return f'String: "{truncate_str(self.string)}"'


class IODataFromProgram(IOData):
    """
    Test's IO in a form of a (compiled) program.
    """

    def __init__(self, compile: str, run: str):
        self.compile = compile
        self.run = run
        if self.compile:
            bash_command(self.compile)

    def get_command(self) -> str:
        return f"{self.run}"

    def __str__(self) -> str:
        return f"Program: {self.run}"


def make_data_from_dict(data: dict, config_dir: Path) -> IOData:
    """
    Constructs a IOData object from dict depending on a config.
    Supported types include:
    - File
    - String
    - Run (+ Compile if needed)
    """

    # Special test for Andrzej
    present = set()
    if FILE in data:
        present.add(FILE)
    if STRING in data:
        present.add(STRING)
    if RUN in data:
        present.add(RUN)

    if len(present) > 1:
        raise ValueError("Should contain only one of the following: " + str(present))

    if COMPILE in data and not RUN in data:
        raise ValueError(f"Missing `{RUN}` in {IODataFromProgram.__name__}: {data}")

    if FILE in data:
        path = config_dir / Path(data[FILE])
        if not path.exists():
            raise FileNotFoundError(f"File not found: {path.absolute()}")
        return IODataFromFile(data[FILE])
    elif STRING in data:
        return IODataFromString(data[STRING])
    elif RUN in data:
        return IODataFromProgram(data.get(COMPILE, ""), data[RUN])
    else:
        raise ValueError("Invalid data dictionary: " + str(data))
