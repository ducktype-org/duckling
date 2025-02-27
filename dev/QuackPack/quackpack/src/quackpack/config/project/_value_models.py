import re
from enum import StrEnum
from typing import Annotated

from pydantic import AfterValidator


class PackageLocationImpl(StrEnum):
    GIT = "git"
    LOCAL = "local"
    DUCKNEST = "ducknest"


def check_package_location(value: dict[PackageLocationImpl, str]) -> dict[PackageLocationImpl, str]:
    if len(value) == 0:
        raise ValueError("There should be one location specified")
    if len(value) > 1:
        raise ValueError("There should be just one location specified")
    return value


type PackageLocation = Annotated[dict[PackageLocationImpl, str], AfterValidator(check_package_location)]


def check_legal_chars_package(value: str) -> str:
    regex = r"(|[a-z]|[A-Z]|[0-9]|_)*"
    if re.fullmatch(regex, value) is None:
        raise ValueError("Package names can only consist of letters, digits, '_' or '-'")
    return value


type PackageName = Annotated[str, AfterValidator(check_legal_chars_package)]


def check_legal_chars_venv(value: str) -> str:
    regex = r"([a-z]|[A-Z]|[0-9]|_|\.|-|\(|\)|\[|\]|\{|\}|\<|\>)*"  # TODO decide what more
    if re.fullmatch(regex, value) is None:
        raise ValueError(r"Venv names can only consist of letters, digits, or anything from '_-()[]{}'")
    return value


type VenvName = Annotated[str, AfterValidator(check_legal_chars_venv)]


def check_legal_chars_flag(value: str) -> str:
    regex = r"(|[a-z]|[A-Z]|[0-9]|_)*"
    if re.fullmatch(regex, value) is None:
        raise ValueError("Flag names can only consist of letters, digits, '_' or '-'")
    return value
