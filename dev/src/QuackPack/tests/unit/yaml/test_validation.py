from pathlib import Path
from typing import Annotated, Any
from unittest.mock import mock_open

import pytest
from pydantic import BaseModel, ConfigDict, Discriminator, RootModel, Tag

from quackpack.util.yaml.errors import ConfigFileLoadError
from quackpack.util.yaml.strict_parsing import load_and_validate


def test_deserialises_valid_data(tmp_path: Path):
    class ProperValidationModel(BaseModel):
        key1: int
        key2: list[dict[str, str] | bool]

    DATA = """key1: 5
key2:
- {key3: lorem_ipsum}
- False
"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        _ = load_and_validate(tmp_path, ProperValidationModel)


def test_invalid_discriminator_should_fail(tmp_path: Path):
    DATA = """key1: lorem_ipsum"""

    def discriminate_fail(v: Any) -> str | None:
        if isinstance(v, int):
            return "!int"
        if isinstance(v, bool):
            return "!bool"
        return None

    class UnionDiscriminatorFailModel(BaseModel):
        key1: Annotated[
            (Annotated[int, Tag("!int")] | Annotated[bool, Tag("INVALID TAG")]),
            Discriminator(
                discriminate_fail,
                custom_error_type="invalid_union_type",
                custom_error_message="Example error message",
            ),
        ]

    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, UnionDiscriminatorFailModel)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]key1: [white on red]lorem_ipsum[/white on red]

Example error message"""
        )


def test_chosen_model_should_fail_with_invalid_data(tmp_path: Path):
    DATA = """key1: {key2: lorem_ipsum}
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

    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, UnionChosenOptionFailModel)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]key1: {{key2: [white on red]lorem_ipsum[/white on red]}}

Input should be a valid integer, unable to parse string as an integer"""
        )


def test_data_with_missing_key_should_fail(tmp_path: Path):
    DATA = """key1: {key2: lorem_ipsum}
"""

    class MissingKeyHelperModel(BaseModel):
        key2: str
        key3: str

    class MissingKeyModel(BaseModel):
        key1: MissingKeyHelperModel

    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, MissingKeyModel)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue][white on red]key1[/white on red]: {{key2: lorem_ipsum}}

Obligatory field <key3> absent from this section"""
        )


def test_not_a_dict_shouldnt_deserialise(tmp_path: Path):
    DATA = """[a, b, c]"""

    class NoDictModel(BaseModel):
        pass

    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, NoDictModel)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] While trying to parse the file: [green]{tmp_path!s}[/green]

Your configuration file should deserialize to a dictionary"""
        )


def test_no_value_should_fail(tmp_path: Path):
    DATA = """key1:"""

    class NoValueHelperModel(BaseModel):
        dummy_key: str

    class NoValueModel(BaseModel):
        key1: NoValueHelperModel

    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, NoValueModel)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue][white on red]key1[/white on red]:

Key should have a value assigned"""
        )


def test_data_with_extra_keys_should_fail(tmp_path: Path):
    DATA = """key1:
  key2: a
  key3: b"""

    class ExtraKeyHelperModel(BaseModel):
        model_config = ConfigDict(extra="forbid")
        key3: str

    class ExtraKeyModel(BaseModel):
        key1: ExtraKeyHelperModel

    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, ExtraKeyModel)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]key1:
[blue]2.[/blue]  [white on red]key2[/white on red]: a
[blue]3.[/blue]  key3: b

Extra inputs are not permitted"""
        )


def test_key_validation_fail(tmp_path: Path):
    KEY_VALIDATION_FAIL_FILE = """key1: {key2: a}"""

    type KeyValidationFailHelperModel = RootModel[dict[int, str]]  # pyright: ignore [reportGeneralTypeIssues]

    class KeyValidationFailModel(BaseModel):
        key1: KeyValidationFailHelperModel

    m = mock_open(read_data=KEY_VALIDATION_FAIL_FILE)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, KeyValidationFailModel)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]key1: {{[white on red]key2[/white on red]: a}}

Input should be a valid integer, unable to parse string as an integer"""
        )
