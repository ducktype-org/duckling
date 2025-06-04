from pathlib import Path

import pytest

from quackpack.project_loader import ProjectLoader
from quackpack.util.errors import QuackPackError


def test_no_project_from_directory(tmp_path: Path):
    with pytest.raises(QuackPackError) as excinfo:
        ProjectLoader.find_from_directory(tmp_path)
    assert str(excinfo.value) == f"No manifest found from {tmp_path} to /"


def test_not_a_dir(tmp_path: Path):
    path = tmp_path / "foo"
    with pytest.raises(NotADirectoryError) as excinfo:
        ProjectLoader.find_from_directory(path)
    assert str(excinfo.value) == str(path)


def test_not_a_file(tmp_path: Path):
    with pytest.raises(FileNotFoundError) as excinfo:
        ProjectLoader.find_from_file(tmp_path)
    assert str(excinfo.value) == str(tmp_path)


def test_manifest_at_cwd(tmp_path: Path):
    manifest = tmp_path / ProjectLoader.MANIFEST_NAME
    manifest.touch()
    project = ProjectLoader.find_from_directory(tmp_path)
    assert project.manifest_path == manifest


def test_manifest_at_parent(tmp_path: Path):
    manifest = tmp_path / ProjectLoader.MANIFEST_NAME
    manifest.touch()
    child = tmp_path / "foo"
    child.mkdir()
    project = ProjectLoader.find_from_directory(child)
    assert project.manifest_path == manifest


def test_manifest_here_from_file(tmp_path: Path):
    manifest = tmp_path / ProjectLoader.MANIFEST_NAME
    manifest.touch()
    project = ProjectLoader.find_from_file(manifest)
    assert project.manifest_path == manifest


def test_manifest_parent_from_file(tmp_path: Path):
    manifest = tmp_path / ProjectLoader.MANIFEST_NAME
    manifest.touch()
    child = tmp_path / "foo"
    child.mkdir()
    childer = child / "xd"
    childer.touch()
    assert childer.parent.parent == tmp_path
    project = ProjectLoader.find_from_file(childer)
    assert project.manifest_path == manifest


def test_manifest_exact_directory(tmp_path: Path):
    manifest = tmp_path / ProjectLoader.MANIFEST_NAME
    manifest.touch()
    project = ProjectLoader.find_at_exact_directory(tmp_path)
    assert project.manifest_path == manifest


def test_manifest_exact_file(tmp_path: Path):
    manifest = tmp_path / ProjectLoader.MANIFEST_NAME
    manifest.touch()
    project = ProjectLoader.find_at_exact_file(manifest)
    assert project.manifest_path == manifest


def test_manifest_exact_directory_notadir(tmp_path: Path):
    foo = tmp_path / "xd"
    assert not foo.exists()
    with pytest.raises(NotADirectoryError) as excinfo:
        ProjectLoader.find_at_exact_directory(foo)
    assert str(excinfo.value) == str(foo)
    foo.touch()
    with pytest.raises(NotADirectoryError) as excinfo:
        ProjectLoader.find_at_exact_directory(foo)
    assert str(excinfo.value) == str(foo)


def test_manifest_exact_file_not_a_file(tmp_path: Path):
    foo = tmp_path / "xd"
    assert not foo.exists()
    with pytest.raises(QuackPackError) as excinfo:
        ProjectLoader.find_at_exact_file(foo)
    assert str(excinfo.value) == f"{foo} is not a manifest"
    foo.mkdir()
    with pytest.raises(QuackPackError) as excinfo:
        ProjectLoader.find_at_exact_file(foo)
    assert str(excinfo.value) == f"{foo} is not a manifest"
    foo.rmdir()
    foo.touch()
    with pytest.raises(QuackPackError) as excinfo:
        ProjectLoader.find_at_exact_file(foo)
    assert str(excinfo.value) == f"{foo} is not a manifest"


def test_manifest_from_file_but_manifest_is_in_cwd(tmp_path: Path):
    manifest = tmp_path / ProjectLoader.MANIFEST_NAME
    manifest.touch()
    foo = tmp_path / "foo"
    foo.touch()
    assert foo != manifest
    with pytest.raises(QuackPackError) as excinfo:
        ProjectLoader.find_from_file(foo)
    assert str(excinfo.value) == f"No manifest found from {tmp_path.parent} to /"
