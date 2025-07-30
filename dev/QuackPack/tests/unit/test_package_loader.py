from pathlib import Path

import pytest

from quackpack.global_context import GlobalContext
from quackpack.package_loader import PackageLoader
from quackpack.util.types.errors import QuackPackError

BASIC_MANIFEST = """
metadata:
  name: foo
  version: 0.1
"""


def test_no_package_from_directory(tmp_path: Path):
    with pytest.raises(QuackPackError) as excinfo:
        PackageLoader.find_from_directory(tmp_path, GlobalContext.default())
    assert str(excinfo.value) == f"No manifest found from `{tmp_path}` to `/`"


def test_not_a_dir(tmp_path: Path):
    path = tmp_path / "foo"
    with pytest.raises(NotADirectoryError) as excinfo:
        PackageLoader.find_from_directory(path, GlobalContext.default())
    assert str(excinfo.value) == str(path)


def test_not_a_file(tmp_path: Path):
    with pytest.raises(FileNotFoundError) as excinfo:
        PackageLoader.find_from_file(tmp_path, GlobalContext.default())
    assert str(excinfo.value) == str(tmp_path)


def test_manifest_at_cwd(tmp_path: Path):
    manifest = tmp_path / PackageLoader.MANIFEST_NAME
    manifest.touch()
    manifest.write_text(BASIC_MANIFEST)
    package = PackageLoader.find_from_directory(tmp_path, GlobalContext.default())
    assert package.manifest_path == manifest


def test_manifest_at_parent(tmp_path: Path):
    manifest = tmp_path / PackageLoader.MANIFEST_NAME
    manifest.touch()
    manifest.write_text(BASIC_MANIFEST)
    child = tmp_path / "foo"
    child.mkdir()
    package = PackageLoader.find_from_directory(child, GlobalContext.default())
    assert package.manifest_path == manifest


def test_manifest_here_from_file(tmp_path: Path):
    manifest = tmp_path / PackageLoader.MANIFEST_NAME
    manifest.touch()
    manifest.write_text(BASIC_MANIFEST)
    package = PackageLoader.find_from_file(manifest, GlobalContext.default())
    assert package.manifest_path == manifest


def test_manifest_parent_from_file(tmp_path: Path):
    manifest = tmp_path / PackageLoader.MANIFEST_NAME
    manifest.touch()
    manifest.write_text(BASIC_MANIFEST)
    child = tmp_path / "foo"
    child.mkdir()
    childer = child / "xd"
    childer.touch()
    assert childer.parent.parent == tmp_path
    package = PackageLoader.find_from_file(childer, GlobalContext.default())
    assert package.manifest_path == manifest


def test_manifest_exact_directory(tmp_path: Path):
    manifest = tmp_path / PackageLoader.MANIFEST_NAME
    manifest.touch()
    manifest.write_text(BASIC_MANIFEST)
    package = PackageLoader.find_at_exact_directory(tmp_path, GlobalContext.default())
    assert package.manifest_path == manifest


def test_manifest_exact_file(tmp_path: Path):
    manifest = tmp_path / PackageLoader.MANIFEST_NAME
    manifest.touch()
    manifest.write_text(BASIC_MANIFEST)
    package = PackageLoader.find_at_exact_file(manifest, GlobalContext.default())
    assert package.manifest_path == manifest


def test_manifest_exact_directory_notadir(tmp_path: Path):
    foo = tmp_path / "xd"
    assert not foo.exists()
    with pytest.raises(NotADirectoryError) as excinfo:
        PackageLoader.find_at_exact_directory(foo, GlobalContext.default())
    assert str(excinfo.value) == str(foo)
    foo.touch()
    with pytest.raises(NotADirectoryError) as excinfo:
        PackageLoader.find_at_exact_directory(foo, GlobalContext.default())
    assert str(excinfo.value) == str(foo)


def test_manifest_exact_file_not_a_file(tmp_path: Path):
    foo = tmp_path / "xd"
    assert not foo.exists()
    with pytest.raises(QuackPackError) as excinfo:
        PackageLoader.find_at_exact_file(foo, GlobalContext.default())
    assert str(excinfo.value) == f"{foo} is not a manifest"
    foo.mkdir()
    with pytest.raises(QuackPackError) as excinfo:
        PackageLoader.find_at_exact_file(foo, GlobalContext.default())
    assert str(excinfo.value) == f"{foo} is not a manifest"
    foo.rmdir()
    foo.touch()
    with pytest.raises(QuackPackError) as excinfo:
        PackageLoader.find_at_exact_file(foo, GlobalContext.default())
    assert str(excinfo.value) == f"{foo} is not a manifest"


def test_manifest_from_file_but_manifest_is_in_cwd(tmp_path: Path):
    manifest = tmp_path / PackageLoader.MANIFEST_NAME
    manifest.touch()
    foo = tmp_path / "foo"
    foo.touch()
    assert foo != manifest
    with pytest.raises(QuackPackError) as excinfo:
        PackageLoader.find_from_file(foo, GlobalContext.default())
    assert str(excinfo.value) == f"No manifest found from `{tmp_path.parent}` to `/`"
