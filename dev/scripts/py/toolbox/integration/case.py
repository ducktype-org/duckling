import abc
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from ..helpers import bash_command, trucate_str


class Data(abc.ABC):
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


class DataFromFile(Data):
    """
    Test's IO from File.
    """

    def __init__(self, file: Path):
        self.file = file

    def get_command(self) -> str:
        return f"cat {self.file}"

    def __str__(self) -> str:
        return f"File: {self.file}"


class DataFromString(Data):
    """
    Test's IO in a form of a string literal.
    """

    def __init__(self, string: str):
        self.string = string

    def get_command(self) -> str:
        return f'echo -ne "{self.string}"'

    def __str__(self) -> str:
        return f'String: "{trucate_str(self.string)}"'


class DataFromProgram(Data):
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


def make_data_from_dict(data: dict, config_dir: Path) -> Data:
    """
    Constructs a Data object from dict depending on a config.
    Supported types include:
    - file
    - string
    - run (+ compile if needed)
    """
    if "file" in data:
        path = config_dir / Path(data["file"])
        if not path.exists():
            raise FileNotFoundError(f"File not found: {path.absolute()}")
        return DataFromFile(data["file"])
    elif "string" in data:
        return DataFromString(data["string"])
    elif "run" in data or "compile" in data:
        if not "run" in data:
            raise ValueError(f"Missing `run` in DataFromProgram: {data}")
        return DataFromProgram(data.get("compile", ""), data["run"])
    else:
        raise ValueError("Invalid data dictionary: " + str(data))


@dataclass
class Case:
    """
    Test's Case object that represents data needed to run a test case.
    """

    name: str
    run_args: str
    input: Optional[Data]
    expected_exitcode: int
    expected_output: Optional[Data]
    expected_err: Optional[Data]
    timeout: int
