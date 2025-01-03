import abc
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from ..helpers import bash_command


class Data(abc.ABC):
    @abc.abstractmethod
    def get_command(self) -> str:
        """A shell command to write the data to stdout"""

    @abc.abstractmethod
    def __str__(self) -> str:
        """Type of the data"""


class DataFromFile(Data):
    def __init__(self, file: Path):
        self.file = file

    def get_command(self) -> str:
        return f"cat {self.file}"

    def __str__(self) -> str:
        return f"File: {self.file}"


class DataFromString(Data):
    def __init__(self, string: str):
        self.string = string

    def get_command(self) -> str:
        return f"echo {self.string}"

    def __str__(self) -> str:
        return f"String: {self.string[:10] + ('...' if len(self.string) > 10 else '')}"


class DataFromProgram(Data):
    def __init__(self, compile: str, run: str):
        self.compile = compile
        self.run = run
        if self.compile:
            bash_command(self.compile)

    def get_command(self) -> str:
        return f"{self.run}"

    def __str__(self) -> str:
        return f"Program: {self.run}"


def make_data_from_dict(data: dict) -> Data:
    if "file" in data:
        file = Path(data["file"])
        if not file.exists():
            raise FileNotFoundError(f"File not found: {file}")
        return DataFromFile(file)
    elif "string" in data:
        return DataFromString(data["string"])
    elif "run" in data:
        return DataFromProgram(data.get("compile", ""), data["run"])
    else:
        raise ValueError("Invalid data dictionary: " + str(data))


# https://unix.stackexchange.com/questions/922/cant-pipe-into-diff
# Tee
# pipe output to post_run_command


@dataclass
class RunCase:
    name: str
    run_args: str
    input: Optional[Data]
    expected_exitcode: int
    expected_output: Optional[Data]
    expected_err: Optional[Data]
    post_run: str
