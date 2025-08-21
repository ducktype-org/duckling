from pathlib import Path

from quackpack.core.package_loader import PackageLoader
from quackpack.util.global_context import GlobalContext
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version


def test_basic_manifest(tmp_path: Path):
    MANIFEST_DATA = """
metadata:
  name: foo
  version: 1.2
"""
    manifest = tmp_path / PackageLoader.MANIFEST_NAME
    manifest.touch()
    with open(manifest, "w") as f:
        f.write(MANIFEST_DATA)
    project = PackageLoader.find_from_directory(tmp_path, GlobalContext.default())
    assert project.manifest_path == manifest
    assert project.manifest.summary.name == Identifier("foo")
    assert project.manifest.summary.spec.version == Version(1, 2)
