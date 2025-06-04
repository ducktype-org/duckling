from pathlib import Path
from unittest.mock import mock_open

import pytest
from pydantic import ValidationError

from quackpack.config.project import DependencyConditions, DependencyEntry, GitEntry, Metadata, VersionList
from quackpack.project import Project
from quackpack.util.deser.deserialize import load_and_validate
from quackpack.util.deser.errors import ConfigFileLoadError
from quackpack.util.pkgid import Identifier
from quackpack.util.version import Version


def test_single_version():
    DATA = {"version": "3.4"}
    validated_model = DependencyEntry.model_validate(DATA)
    assert isinstance(validated_model.version, VersionList)
    assert validated_model.version[0] == Version(3, 4)


def test_multiple_ored_versions():
    DATA = {"version": "3.4 or 2.1.3 or 6"}
    validated_model = DependencyEntry.model_validate(DATA)
    assert isinstance(validated_model.version, VersionList)
    assert validated_model.version[0] == Version(3, 4)
    assert validated_model.version[1] == Version(2, 1, 3)
    assert validated_model.version[2] == Version(6)


def test_versions_list():
    DATA = {"version": ["3.4", "2.1.3"]}
    validated_model = DependencyEntry.model_validate(DATA)
    assert isinstance(validated_model.version, VersionList)
    assert validated_model.version[0] == Version(3, 4)
    assert validated_model.version[1] == Version(2, 1, 3)


def test_dependency_entry_with_flags():
    DATA = {"version": "1.2.3", "flags": [Identifier("a"), {Identifier("b"): {"system": "Darwin"}}]}
    validated_model = DependencyEntry.model_validate(DATA)
    assert isinstance(validated_model.flags, list)
    assert isinstance(validated_model.flags[0], Identifier)
    assert isinstance(validated_model.flags[1], dict)
    assert Identifier("b") in validated_model.flags[1]
    assert isinstance(validated_model.flags[1][Identifier("b")], DependencyConditions)
    assert validated_model.flags[1][Identifier("b")].system == "Darwin"


def test_not_dict_metadata_should_fail():
    DATA = ["lorem ipsum", 2]
    with pytest.raises(ValidationError) as _:
        _ = Metadata.model_validate(DATA)


def test_not_dict_git_should_fail():
    DATA = ["lorem ipsum", 2]
    with pytest.raises(ValidationError) as _:
        _ = GitEntry.model_validate(DATA)


def test_not_dict_dependency_should_fail():
    DATA = ["lorem ipsum", 2]
    with pytest.raises(ValidationError) as _:
        _ = DependencyEntry.model_validate(DATA)


def test_invalid_version_should_fail(tmp_path: Path):
    DATA = """version: lorem ipsum"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]version: [white on red]lorem ipsum[/white on red]

Value error, invalid literal for int() with base 10: 'l'"""
        )


def test_invalid_versions_list_should_fail(tmp_path: Path):
    DATA = """version: [2.3, lorem ipsum]"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]version: [2.3, [white on red]lorem ipsum[/white on red]]

Value error, invalid literal for int() with base 10: 'lorem ipsum'"""
        )


def test_invalid_versions_list_type_should_fail(tmp_path: Path):
    DATA = """version: [2.3, {a: b}]"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]version: [2.3, [white on red]{{a: b}}[/white on red]]

Value error, value should be a string"""
        )


def test_git_with_too_many_fields_should_fail(tmp_path: Path):
    DATA = """version:
  git_url: https://google.com/
  tag: tag
  branch: br
"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]version:
[blue]2.[/blue]  [white on red]git_url: https://google.com/[/white on red]
[blue]3.[/blue][white on red]  tag: tag[/white on red]
[blue]4.[/blue][white on red]  branch: br[/white on red]

Value error, Expected at most one of 'tag' or 'branch'"""
        )


def test_invalid_type_in_flags_should_fail(tmp_path: Path):
    DATA = """version: 1.2.3
flags:
- a
- b: [bad_value]"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        print(DATA)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]3.[/blue]- a
[blue]4.[/blue]- b: [white on red][bad_value][/white on red]

Input should be a valid dictionary or instance of DependencyConditions"""
        )


def test_dependency_entry_flags_no_discriminator_fail(tmp_path: Path):
    DATA = """version: 1.2.3
flags:
- [a, b]"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]2.[/blue]flags:
[blue]3.[/blue][white on red]- [a, b][/white on red]

Value should be a list of strings or mappings from strings to DependencyConditions"""
        )


def test_empty_versions_list_should_fail(tmp_path: Path):
    DATA = """version: []"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]version: [white on red][][/white on red]

Value error, Empty dependency list"""
        )


def test_unneeded_server_url_should_fail(tmp_path: Path):
    DATA = """version:
  git_url: https://www.google.pl
server_url: https://www.google.pl"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue][white on red]version:[/white on red]
[blue]2.[/blue][white on red]  git_url: https://www.google.pl[/white on red]
[blue]3.[/blue][white on red]server_url: https://www.google.pl[/white on red]

Value error, The server_url field should be blank, as the dependency is not a server dependency."""
        )


def test_quackpack_config_serialisation(tmpdir: Path, tmp_path: Path):
    INPUT = f"""
metadata:
  name: any_name
  license: do anything
  author: Baltazar Gabka
  version: 1.0
  compiler_version: 3.1.2

dependencies:
  dep1:
    version: [1.2.3, 2.3.4, 5]
  dep2:
    version:
      git_url: https://google.com/
      branch: stable
  dep3:
    version:
      path: {tmpdir!s}
  """
    EXPECTED = f"""metadata:
  author: Baltazar Gabka
  version: 1.0.0
  name: any_name
  license: do anything
  compiler_version: 3.1.2
dependencies:
  dep1:
    version: 1.2.3 or 2.3.4 or 5.0.0
  dep2:
    version:
      git_url: https://google.com/
      branch: stable
  dep3:
    version:
      path: {tmpdir!s}
"""
    m = mock_open(read_data=INPUT)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        venv = Project(tmp_path)
        venv.save_to_disk()

    handle = m()
    output = "".join(x.args[0] for x in handle.write.call_args_list)
    assert output == EXPECTED


def test_not_a_url(tmp_path: Path):
    DATA = """version:
  git_url: Ala ma kota
"""
    m = mock_open(read_data=DATA)

    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)

        with pytest.raises(ConfigFileLoadError) as exc_info:
            _ = load_and_validate(tmp_path, DependencyEntry)
        assert (
            str(exc_info.value)
            == f"""[bold red]ERROR![/bold red] in the file: [green]{tmp_path!s}[/green]

[blue]1.[/blue]version:
[blue]2.[/blue]  git_url: [white on red]Ala ma kota[/white on red]

Input should be a valid URL, relative URL without a base"""
        )
