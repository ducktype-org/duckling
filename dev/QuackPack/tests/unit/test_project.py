from pathlib import Path

from quackpack.project_loader import ProjectLoader
from quackpack.util.version import Version


def test_basic_manifest(tmp_path: Path):
    MANIFEST_DATA = """
metadata:
  name: foo
  version: 1.2
"""
    manifest = tmp_path / ProjectLoader.MANIFEST_NAME
    manifest.touch()
    with open(manifest, "w") as f:
        f.write(MANIFEST_DATA)
    project = ProjectLoader.find_from_directory(tmp_path)
    assert project.manifest_path == manifest
    assert project.manifest_without_acquiring_lock().metadata.name == "foo"
    assert project.manifest_without_acquiring_lock().metadata.version == Version(1, 2)
