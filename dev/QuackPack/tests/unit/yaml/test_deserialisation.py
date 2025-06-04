from pathlib import Path
from unittest.mock import mock_open

import pytest

from quackpack.util.deser.errors import ConfigFileLoadError
from quackpack.util.deser.yaml.strict_parsing import strict_safe_load


def test_successful_deserialization_and_positions(tmp_path: Path):
    DATA = """key1:
- key2: {key5: [b, c]}
- key3:
  - key4: d
- a

key6: e"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        positions, deserialized_object = strict_safe_load(tmp_path)
        assert "key1" in deserialized_object
        assert isinstance(deserialized_object["key1"], list) and len(deserialized_object["key1"]) == 3
        assert "key2" in deserialized_object["key1"][0]
        assert "key5" in deserialized_object["key1"][0]["key2"]
        assert deserialized_object["key1"][0]["key2"]["key5"][0] == "b"
        assert deserialized_object["key1"][0]["key2"]["key5"][1] == "c"
        assert "key3" in deserialized_object["key1"][1]
        assert "key4" in deserialized_object["key1"][1]["key3"][0]
        assert deserialized_object["key1"][1]["key3"][0]["key4"] == "d"
        assert deserialized_object["key1"][2] == "a"
        assert "key6" in deserialized_object
        assert deserialized_object["key6"] == "e"

        assert positions["key1", 0, "!0"][0].line == 1 and positions["key1", 0, "!0"][0].column == 0
        assert positions["key1", 0, "!0"][1].line == 1 and positions["key1", 0, "!0"][1].column == 1

        assert (
            positions["key1", 0, "key2", "", "!1"][0].line == 1
            and positions["key1", 0, "key2", "", "!1"][0].column == 8
        )
        assert (
            positions["key1", 0, "key2", "", "!1"][1].line == 1
            and positions["key1", 0, "key2", "", "!1"][1].column == 9
        )

        assert (
            positions["key1", 0, "key2", "key5", 0, "!2"][0].line == 1
            and positions["key1", 0, "key2", "key5", 0, "!2"][0].column == 15
        )
        assert (
            positions["key1", 0, "key2", "key5", 0, "!2"][1].line == 1
            and positions["key1", 0, "key2", "key5", 0, "!2"][1].column == 16
        )

        assert (
            positions["key1", 0, "key2", "key5", "!3"][0].line == 1
            and positions["key1", 0, "key2", "key5", "!3"][0].column == 20
        )
        assert (
            positions["key1", 0, "key2", "key5", "!3"][1].line == 1
            and positions["key1", 0, "key2", "key5", "!3"][1].column == 21
        )

        assert (
            positions["key1", 0, "key2", "!4"][0].line == 1
            and positions["key1", 0, "key2", "!4"][0].column == 21
        )
        assert (
            positions["key1", 0, "key2", "!4"][1].line == 1
            and positions["key1", 0, "key2", "!4"][1].column == 22
        )

        assert (
            positions["key1", 1, "key3", 0, "!5"][0].line == 3
            and positions["key1", 1, "key3", 0, "!5"][0].column == 2
        )
        assert (
            positions["key1", 1, "key3", 0, "!5"][1].line == 3
            and positions["key1", 1, "key3", 0, "!5"][1].column == 3
        )

        assert (
            positions["key1", 1, "key3", 0, "key4", "[key]"][0].line == 3
            and positions["key1", 1, "key3", 0, "key4", "[key]"][0].column == 4
        )
        assert positions["key1", 2][1].line == 4 and positions["key1", 2][1].column == 3
        assert (
            positions["key1", 0, "key2", "key5", 1][0].line == 1
            and positions["key1", 0, "key2", "key5", 1][0].column == 19
        )
        assert positions["key6", "[key]"][1].line == 6 and positions["key6", "[key]"][1].column == 4
        assert (
            positions["key1", 1, "key3", 0, "key4"][0].line == 3
            and positions["key1", 1, "key3", 0, "key4"][0].column == 10
        )
        assert (
            positions["key1", 0, "key2", "key5", "[key]"][1].line == 1
            and positions["key1", 0, "key2", "key5", "[key]"][1].column == 13
        )


def test_implicit_list(tmp_path: Path):
    DATA = """key1:
- [a, b]
- c
"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        positions, _ = strict_safe_load(tmp_path)

        assert positions["key1", 0, 0][0].line == 1 and positions["key1", 0, 0][0].column == 3
        assert positions["key1", 0, 0][1].line == 1 and positions["key1", 0, 0][1].column == 4

        assert positions["key1", 1][0].line == 2 and positions["key1", 1][0].column == 2
        assert positions["key1", 1][1].line == 2 and positions["key1", 1][1].column == 3

        assert positions["key1", 0, "!0"][0].line == 1 and positions["key1", 0, "!0"][0].column == 0
        assert positions["key1", 0, "!0"][1].line == 1 and positions["key1", 0, "!0"][1].column == 1

        assert positions["key1", 0, 0, "!1"][0].line == 1 and positions["key1", 0, 0, "!1"][0].column == 2
        assert positions["key1", 0, 0, "!1"][1].line == 1 and positions["key1", 0, 0, "!1"][1].column == 3

        assert positions["key1", 0, "!2"][0].line == 1 and positions["key1", 0, "!2"][0].column == 7
        assert positions["key1", 0, "!2"][1].line == 1 and positions["key1", 0, "!2"][1].column == 8


def test_implicit_dict(tmp_path: Path):
    DATA = """[a, key1: [b], key2: {key3: c}, d]
"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        positions, _ = strict_safe_load(tmp_path)

        assert positions[0,][0].line == 0 and positions[0,][0].column == 1
        assert positions[0,][1].line == 0 and positions[0,][1].column == 2

        assert positions[1, "key1", 0][0].line == 0 and positions[1, "key1", 0][0].column == 11
        assert positions[1, "key1", 0][1].line == 0 and positions[1, "key1", 0][1].column == 12

        assert (
            positions[2, "key2", "key3", "[key]"][0].line == 0
            and positions[2, "key2", "key3", "[key]"][0].column == 22
        )
        assert (
            positions[2, "key2", "key3", "[key]"][1].line == 0
            and positions[2, "key2", "key3", "[key]"][1].column == 26
        )

        assert positions[3,][0].line == 0 and positions[3,][0].column == 32
        assert positions[3,][1].line == 0 and positions[3,][1].column == 33

        assert positions[0, "!0"][0].line == 0 and positions[0, "!0"][0].column == 0
        assert positions[0, "!0"][1].line == 0 and positions[0, "!0"][1].column == 1

        assert positions[2, "!4"][0].line == 0 and positions[2, "!4"][0].column == 29
        assert positions[2, "!4"][1].line == 0 and positions[2, "!4"][1].column == 30

        assert positions["!5",][0].line == 0 and positions["!5",][0].column == 33
        assert positions["!5",][1].line == 0 and positions["!5",][1].column == 34


def test_invalid_deserialisation_should_fail(tmp_path: Path):
    DATA = """key1:
- key2: {key5: [b, c]}
- key3:
  - [key4, key7]: d
- a

key6: e"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            strict_safe_load(tmp_path)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]3.[/blue]- key3:
[blue]4.[/blue]  - [white on red][[/white on red]key4, key7]: d
[blue]5.[/blue]- a

Failed to deserialize the file"""
        )


def test_forbidden_yaml_syntax_should_fail(tmp_path: Path):
    DATA = """key1:
- key2: {key5: [b, c]}
- key3: &remember_me
  - key4: d
- a

key6: e"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            strict_safe_load(tmp_path)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]2.[/blue]- key2: {{key5: [b, c]}}
[blue]3.[/blue]- key3: [white on red]&remember_me[/white on red]
[blue]4.[/blue]  - key4: d

This YAML syntax is not proper for a Quack Pack config file"""
        )


def test_forbidden_yaml_syntax_should_fail_and_empty_lines(tmp_path: Path):
    DATA = """key1:
- key2: {key5: [b, c]}
- key3: &remember_me

key6: e"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            strict_safe_load(tmp_path)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]2.[/blue]- key2: {{key5: [b, c]}}
[blue]3.[/blue]- key3: [white on red]&remember_me[/white on red]
[blue]4.[/blue]

This YAML syntax is not proper for a Quack Pack config file"""
        )
