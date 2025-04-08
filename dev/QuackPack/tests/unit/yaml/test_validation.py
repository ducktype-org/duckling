from pathlib import Path
from typing import Annotated, Any
from unittest.mock import mock_open

import pytest
from pydantic import BaseModel, ConfigDict, Discriminator, RootModel, Tag

from quackpack.util.deser.deserialize import load_and_validate
from quackpack.util.deser.errors import ConfigFileLoadError

# Assets for test 1
PROPERLY_VALIDATED_FILE = """key1: 5
key2:
- {key3: lorem_ipsum}
- False
"""


class ProperValidationModel(BaseModel):
    key1: int
    key2: list[dict[str, str] | bool]


# Assets for test 2
UNION_DISCRIMINATOR_FAIL_FILE = """key1: lorem_ipsum"""


def discriminate_fail(v: Any) -> str | None:
    if isinstance(v, int):
        return "!int"
    if isinstance(v, bool):
        return "!bool"
    return None


class UnionDiscriminatorFailModel(BaseModel):
    key1: Annotated[
        (Annotated[int, Tag("!int")] | Annotated[bool, Tag("bool")]),
        Discriminator(
            discriminate_fail,
            custom_error_type="invalid_union_type",
            custom_error_message="Example error message",
        ),
    ]


# Assets for test 3
UNION_CHOSEN_OPTION_FAIL_FILE = """key1: {key2: lorem_ipsum}
"""


class UnionChosenOptionFailHelperModel(BaseModel):
    key2: int


def discriminate_choose_helper(v: Any) -> str | None:
    if isinstance(v, dict) and "key2" in v:
        return "!helper"
    if isinstance(v, int):
        return "!int"
    return None


class UnionChosenOptionFailModel(BaseModel):
    key1: Annotated[
        (Annotated[int, Tag("!int")] | Annotated[UnionChosenOptionFailHelperModel, Tag("!helper")]),
        Discriminator(discriminate_choose_helper),
    ]


# Assets for test 4
MISSING_KEY_FILE = """key1: {key2: lorem_ipsum}
"""


class MissingKeyHelperModel(BaseModel):
    key2: str
    key3: str


class MissingKeyModel(BaseModel):
    key1: MissingKeyHelperModel


# Assets for test 5
NO_DICT_FILE = """[a, b, c]"""


class NoDictModel(BaseModel):
    pass


# Assets for test 6
NO_VALUE_FILE = """key1:"""


class NoValueHelperModel(BaseModel):
    dummy_key: str


class NoValueModel(BaseModel):
    key1: NoValueHelperModel


# Assets for test 7
EXTRA_KEY_FILE = """key1:
  key2: a
  key3: b"""


class ExtraKeyHelperModel(BaseModel):
    model_config = ConfigDict(extra="forbid")
    key3: str


class ExtraKeyModel(BaseModel):
    key1: ExtraKeyHelperModel


# Assets for test 8
KEY_VALIDATION_FAIL_FILE = """key1: {key2: a}"""

type KeyValidationFailHelperModel = RootModel[dict[int, str]]


class KeyValidationFailModel(BaseModel):
    key1: KeyValidationFailHelperModel


# Tests
class TestValidation:
    def test_validation_positive(self, tmp_path: Path):
        m = mock_open(read_data=PROPERLY_VALIDATED_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            _ = load_and_validate(tmp_path, ProperValidationModel)

    def test_discriminator_fail(self, tmp_path: Path):
        m = mock_open(read_data=UNION_DISCRIMINATOR_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, UnionDiscriminatorFailModel)
            assert (
                str(exc_info.value)
                == "[bold red]ERROR![/bold red] in the file: [green]dummy[/green]\n\n[blue]1.[/blue]key1: [white on red]lorem_ipsum[/white on red] \n\nExample error message".replace(
                    "dummy", str(tmp_path)
                )
            )

    def test_chosen_option_fail(self, tmp_path: Path):
        m = mock_open(read_data=UNION_CHOSEN_OPTION_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, UnionChosenOptionFailModel)
            assert (
                str(exc_info.value)
                == "[bold red]ERROR![/bold red] in the file: [green]dummy[/green]\n\n[blue]1.[/blue]key1: {key2: [white on red]lorem_ipsum[/white on red]} \n\nInput should be a valid integer, unable to parse string as an integer".replace(
                    "dummy", str(tmp_path)
                )
            )

    def test_missing_key_fail(self, tmp_path: Path):
        m = mock_open(read_data=MISSING_KEY_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, MissingKeyModel)
            assert (
                str(exc_info.value)
                == "[bold red]ERROR![/bold red] in the file: [green]dummy[/green]\n\n[blue]1.[/blue][white on red]key1[/white on red]: {key2: lorem_ipsum} \n\nObligatory field <key3> absent from this section".replace(
                    "dummy", str(tmp_path)
                )
            )

    def test_no_dict_fail(self, tmp_path: Path):
        m = mock_open(read_data=NO_DICT_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, NoDictModel)
            assert (
                str(exc_info.value)
                == "[bold red]ERROR![/bold red] While trying to parse the file: [green]dummy[/green]\n\nYour configuration file should deserialize to a dictionary".replace(
                    "dummy", str(tmp_path)
                )
            )

    def test_no_value_fail(self, tmp_path: Path):
        m = mock_open(read_data=NO_VALUE_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, NoValueModel)
            assert (
                str(exc_info.value)
                == "[bold red]ERROR![/bold red] in the file: [green]dummy[/green]\n\n[blue]1.[/blue][white on red]key1[/white on red]: \n\nKey should have a value assigned".replace(
                    "dummy", str(tmp_path)
                )
            )

    def test_extra_key_fail(self, tmp_path: Path):
        m = mock_open(read_data=EXTRA_KEY_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, ExtraKeyModel)
            assert (
                str(exc_info.value)
                == "[bold red]ERROR![/bold red] in the file: [green]dummy[/green]\n\n[blue]1.[/blue]key1: \n[blue]2.[/blue]  [white on red]key2[/white on red]: a \n[blue]3.[/blue]  key3: b \n\nExtra inputs are not permitted".replace(
                    "dummy", str(tmp_path)
                )
            )

    def test_key_validation_fail(self, tmp_path: Path):
        m = mock_open(read_data=KEY_VALIDATION_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, KeyValidationFailModel)
            assert (
                str(exc_info.value)
                == "[bold red]ERROR![/bold red] in the file: [green]dummy[/green]\n\n[blue]1.[/blue]key1: {[white on red]key2[/white on red]: a} \n\nInput should be a valid integer, unable to parse string as an integer".replace(
                    "dummy", str(tmp_path)
                )
            )
