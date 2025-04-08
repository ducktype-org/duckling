# FIXME: Error results have trailing spaces, which are included by `load_and_validate()`.
from pathlib import Path
from unittest.mock import mock_open

import pytest
from pydantic import ValidationError

from quackpack.config.project import Venv
from quackpack.config.project.models import (
    DependencyConditions,
    DependencyEntry,
    GitEntry,
    Metadata,
    VersionList,
)
from quackpack.util.deser.deserialize import load_and_validate
from quackpack.util.deser.errors import ConfigFileLoadError
from quackpack.util.version import Version

SINGLE_VERSION_ENTRY = {"version": "3.4"}

MULTIPLE_VERSIONS_ORED_ENTRY = {"version": "3.4 or 2.1.3 or 6"}

MULTIPLE_VERSIONS_LIST_ENTRY = {"version": ["3.4", "2.1.3"]}

DEPENDENCY_ENTRY_FLAGS_FILE = {"version": "1.2.3", "flags": ["a", {"b": {"system": "Darwin"}}]}

METADATA_NOT_DICT_FAIL = ["lorem ipsum", 2]

GIT_ENTRY_NOT_DICT_FAIL = ["lorem ipsum", 2]

DEPENDENCY_ENTRY_NOT_DICT_FAIL = ["lorem ipsum", 2]

DEPENDENCY_ENTRY_VERSION_STR_FAIL_FILE = """version: lorem ipsum"""

DEPENDENCY_ENTRY_VERSION_LIST_FAIL_FILE = """version: [2.3, lorem ipsum]"""

DEPENDENCY_ENTRY_VERSION_LIST_ELEMENT_NOT_STR_FAIL_FILE = """version: [2.3, {a: b}]"""

DEPENDENCY_ENTRY_VERSION_GIT_MULTIPLE_FIELDS_FAIL_FILE = """version:
  git_url: https://google.com/
  commit: com
  branch: br
"""

DEPENDENCY_ENTRY_FLAGS_DISCRIMINATOR_DICT_FAIL_FILE = """version: 1.2.3
flags:
- a
- b: [bad_value]"""

DEPENDENCY_ENTRY_FLAGS_NO_DISCRIMINATOR_FAIL_FILE = """version: 1.2.3
flags:
- [a, b]"""

DEPENDENCY_ENTRY_VERSION_STR_EMPTY_FAIL_FILE = """version: []"""

# Assets for serialisation tests
QUACKPACK_CONFIG_FILE = """
metadata:
  name: any_name
  license: do anything
  id: no_id
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
      path: SEDHERE
  """

QUACKPACK_DUMPED_CONFIG = """metadata:
  author: Baltazar Gabka
  version: 1.0.0
  id: no_id
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
      path: SEDHERE
"""

NOT_A_GIT_URL = """version:
  git_url: Ala ma kota
"""


class TestModels:
    def test_single_version_entry(self):
        validated_model = DependencyEntry.model_validate(SINGLE_VERSION_ENTRY)
        assert isinstance(validated_model.version, VersionList)
        assert validated_model.version[0] == Version(3, 4)

    def test_multiple_versions_ored_entry(self):
        validated_model = DependencyEntry.model_validate(MULTIPLE_VERSIONS_ORED_ENTRY)
        assert isinstance(validated_model.version, VersionList)
        assert validated_model.version[0] == Version(3, 4)
        assert validated_model.version[1] == Version(2, 1, 3)
        assert validated_model.version[2] == Version(6)

    def test_multiple_versions_list_entry(self):
        validated_model = DependencyEntry.model_validate(MULTIPLE_VERSIONS_LIST_ENTRY)
        assert isinstance(validated_model.version, VersionList)
        assert validated_model.version[0] == Version(3, 4)
        assert validated_model.version[1] == Version(2, 1, 3)

    def test_dependency_entry_flags(self):
        validated_model = DependencyEntry.model_validate(DEPENDENCY_ENTRY_FLAGS_FILE)
        assert isinstance(validated_model.flags, list)
        assert isinstance(validated_model.flags[0], str)
        assert isinstance(validated_model.flags[1], dict)
        assert "b" in validated_model.flags[1]
        assert isinstance(validated_model.flags[1]["b"], DependencyConditions)
        assert validated_model.flags[1]["b"].system == "Darwin"

    def test_metadata_not_dict_fail(self):
        with pytest.raises(ValidationError) as _:
            _ = Metadata.model_validate(METADATA_NOT_DICT_FAIL)

    def test_git_entry_not_dict_fail(self):
        with pytest.raises(ValidationError) as _:
            _ = GitEntry.model_validate(GIT_ENTRY_NOT_DICT_FAIL)

    def test_dependency_entry_not_dict_fail(self):
        with pytest.raises(ValidationError) as _:
            _ = DependencyEntry.model_validate(DEPENDENCY_ENTRY_NOT_DICT_FAIL)

    def test_dependency_entry_version_str_fail(self, tmp_path: Path):
        m = mock_open(read_data=DEPENDENCY_ENTRY_VERSION_STR_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, DependencyEntry)
            assert (
                str(exc_info.value)
                == """[bold red]ERROR![/bold red] in the file: [green]dummy[/green]

[blue]1.[/blue]version: [white on red]lorem ipsum[/white on red] 

Value error, invalid literal for int() with base 10: 'l'""".replace("dummy", str(tmp_path))
            )

    def test_dependency_entry_version_list_fail(self, tmp_path: Path):
        m = mock_open(read_data=DEPENDENCY_ENTRY_VERSION_LIST_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, DependencyEntry)
            assert (
                str(exc_info.value)
                == """[bold red]ERROR![/bold red] in the file: [green]dummy[/green]

[blue]1.[/blue]version: [2.3, [white on red]lorem ipsum[/white on red]] 

Value error, invalid literal for int() with base 10: 'lorem ipsum'""".replace("dummy", str(tmp_path))
            )

    def test_dependency_entry_version_list_not_str_fail(self, tmp_path: Path):
        m = mock_open(read_data=DEPENDENCY_ENTRY_VERSION_LIST_ELEMENT_NOT_STR_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, DependencyEntry)
            assert (
                str(exc_info.value)
                == """[bold red]ERROR![/bold red] in the file: [green]dummy[/green]

[blue]1.[/blue]version: [2.3, [white on red]{a: b}[/white on red]] 

Value error, value should be a string""".replace("dummy", str(tmp_path))
            )

    def test_dependency_entry_version_git_multiple_fields_fail(self, tmp_path: Path):
        m = mock_open(read_data=DEPENDENCY_ENTRY_VERSION_GIT_MULTIPLE_FIELDS_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, DependencyEntry)
            assert (
                str(exc_info.value)
                == """[bold red]ERROR![/bold red] in the file: [green]dummy[/green]

[blue]1.[/blue]version: 
[blue]2.[/blue]  [white on red]git_url: https://google.com/ [/white on red]
[blue]3.[/blue][white on red]  commit: com [/white on red]
[blue]4.[/blue][white on red]  branch: br[/white on red] 

Value error, Expected at most one of 'commit', 'tag', 'branch'""".replace("dummy", str(tmp_path))
            )

    def test_dependency_entry_flags_discriminator_dict_fail(self, tmp_path: Path):
        m = mock_open(read_data=DEPENDENCY_ENTRY_FLAGS_DISCRIMINATOR_DICT_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, DependencyEntry)
            assert (
                str(exc_info.value)
                == """[bold red]ERROR![/bold red] in the file: [green]dummy[/green]

[blue]3.[/blue]- a 
[blue]4.[/blue]- b: [white on red][bad_value][/white on red] 

Input should be a valid dictionary or instance of DependencyConditions""".replace("dummy", str(tmp_path))
            )

    def test_dependency_entry_flags_no_discriminator_fail(self, tmp_path: Path):
        m = mock_open(read_data=DEPENDENCY_ENTRY_FLAGS_NO_DISCRIMINATOR_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, DependencyEntry)
            assert (
                str(exc_info.value)
                == """[bold red]ERROR![/bold red] in the file: [green]dummy[/green]

[blue]2.[/blue]flags: 
[blue]3.[/blue][white on red]- [a, b][/white on red] 

Value should be a list of strings or mappings from strings to DependencyConditions""".replace(
                    "dummy", str(tmp_path)
                )
            )

    def test_dependency_entry_version_str_empty_fail(self, tmp_path: Path):
        m = mock_open(read_data=DEPENDENCY_ENTRY_VERSION_STR_EMPTY_FAIL_FILE)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, DependencyEntry)
            print(exc_info.value)
            assert (
                str(exc_info.value)
                == """[bold red]ERROR![/bold red] in the file: [green]dummy[/green]

[blue]1.[/blue]version: [white on red][][/white on red] 

Value error, Empty dependency list""".replace("dummy", str(tmp_path))
            )

    def test_quackpack_config_serialisation(self, tmpdir: Path, tmp_path: Path):
        m = mock_open(read_data=QUACKPACK_CONFIG_FILE.replace("SEDHERE", str(tmpdir)))

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            venv = Venv(tmp_path)
            venv.write_config()

        handle = m()
        output = "".join(x.args[0] for x in handle.write.call_args_list)
        assert output == QUACKPACK_DUMPED_CONFIG.replace("SEDHERE", str(tmpdir))

    def test_not_a_url(self, tmp_path: Path):
        m = mock_open(read_data=NOT_A_GIT_URL)

        with pytest.MonkeyPatch.context() as mpatch:
            mpatch.setattr("builtins.open", m)

            with pytest.raises(ConfigFileLoadError) as exc_info:
                _ = load_and_validate(tmp_path, DependencyEntry)
            assert (
                str(exc_info.value)
                == """[bold red]ERROR![/bold red] in the file: [green]dummy[/green]

[blue]1.[/blue]version: 
[blue]2.[/blue]  git_url: [white on red]Ala ma kota[/white on red] 

Input should be a valid URL, relative URL without a base""".replace("dummy", str(tmp_path))
            )
