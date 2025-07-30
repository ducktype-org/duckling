import sys
from pathlib import Path

import pytest

from quackpack.global_context import GlobalContext
from quackpack.manifest.parse import parse_manifest
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version


def test_parse_metadata(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1
""")
    manifest = parse_manifest(x, GlobalContext.default())
    summary = manifest.summary
    assert str(summary.name) == "xd"
    assert not summary.deps
    assert not summary.dev_deps
    assert not summary.features


def test_invalid_version_manifest(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.0.1
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert (
        str(excinfo.value)
        == f"""[bold red]ERROR![/bold red] in the file: [green]{x!s}[/green]

[blue]3.[/blue]  name: xd
[blue]4.[/blue]  version: [white on red]0.0.1[/white on red]

versions of format `0.0.X` are not supported"""
    )


def test_parse_with_deps(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1
""")
    manifest = parse_manifest(x, GlobalContext.default())
    summary = manifest.summary
    assert len(summary.deps) == 1
    dep = summary.deps[Identifier("a")]
    assert len(dep.versions) == 1
    assert dep.versions[0] == Version(0, 1)
    assert dep.source.is_registry
    assert not dep.features
    assert not summary.dev_deps
    assert not summary.features


def test_parse_with_dep_or_versions(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1 or 2
""")
    manifest = parse_manifest(x, GlobalContext.default())
    summary = manifest.summary
    assert len(summary.deps) == 1
    dep = summary.deps[Identifier("a")]
    assert len(dep.versions) == 2
    assert dep.versions[0] == Version(0, 1)
    assert dep.versions[1] == Version(2)
    assert dep.source.is_registry
    assert not dep.features
    assert not summary.dev_deps
    assert not summary.features


def test_parse_with_git_dep(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1 or 2
    source:
      git_url: git
""")
    manifest = parse_manifest(x, GlobalContext.default())
    summary = manifest.summary
    assert len(summary.deps) == 1
    dep = summary.deps[Identifier("a")]
    assert len(dep.versions) == 2
    assert dep.versions[0] == Version(0, 1)
    assert dep.versions[1] == Version(2)
    assert dep.source.is_git
    assert dep.source.as_git().git_url == "git"
    assert not summary.dev_deps
    assert not summary.features


def test_parse_with_extra_fields_fail(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1 or 2
    source:
      tag: xd
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert (
        str(excinfo.value)
        == "expected dependency `dependencies.a` to not be a git dependency, but field `dependencies.a.source.tag` is set"
    )


def test_fail_registry_without_version(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    source:
      registry_url: xd
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert (
        str(excinfo.value)
        == "dependency `dependencies.a` is a registry dependency, but does not provide `dependencies.a.version` field"
    )


def test_parse_local_dep_with_versions(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1
    source:
      path: xd
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert (
        str(excinfo.value)
        == "can't determine type of dependency `dependencies.a`, please remove one of fields `dependencies.a.version` or `dependencies.a.source.path`"
    )


def test_fail_no_metadata(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
dependencies:
  a:
    source:
      path: xd
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert str(excinfo.value) == "missing obligatory section `metadata`"


def test_fail_no_metadata_name(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  version: 0.1

dependencies:
  a:
    source:
      path: xd
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert str(excinfo.value) == "missing obligatory key `metadata.name`"


def test_fail_no_metadata_version(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd

dependencies:
  a:
    source:
      path: xd
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert str(excinfo.value) == "missing obligatory key `metadata.version`"


def test_parse_deps_sources(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    source:
      path: xd
  a1:
    source:
      path: ../xd
  a2:
    source:
      path: ~/xd
  a3:
    source:
      path: /xd
  b:
    version: 0.1
  c:
    version: 0.1
    source:
      name: alias
  d:
    version: 0.1
    source:
      name: alias
      registry_url: xd
  e:
    source:
      git_url: git
      branch: branch
      commit: commit
""")
    m = parse_manifest(x, GlobalContext.default())
    summary = m.summary
    assert len(summary.deps) == 8

    a = summary.deps[Identifier("a")]
    assert a.source.is_local
    assert a.source.as_local().absolute_dir_root == tmp_path / "xd"
    assert not a.versions
    assert a.real_name == a.manifest_name

    a1 = summary.deps[Identifier("a1")]
    assert a1.source.is_local
    assert a1.source.as_local().absolute_dir_root == tmp_path.parent / "xd"

    a2 = summary.deps[Identifier("a2")]
    assert a2.source.is_local
    assert a2.source.as_local().absolute_dir_root == Path.home() / "xd"

    a3 = summary.deps[Identifier("a3")]
    assert a3.source.is_local
    assert a3.source.as_local().absolute_dir_root == Path("/xd")

    registry_url = GlobalContext.default().registry_url()

    b = summary.deps[Identifier("b")]
    assert b.source.is_registry
    assert b.source.as_registry().registry_url == registry_url
    assert len(b.versions) == 1
    assert b.versions[0] == Version(0, 1)
    assert b.real_name == b.manifest_name

    c = summary.deps[Identifier("c")]
    assert c.source.is_registry
    assert c.source.as_registry().registry_url == registry_url
    assert len(c.versions) == 1
    assert c.versions[0] == Version(0, 1)
    assert c.real_name == Identifier("alias")
    assert c.manifest_name == Identifier("c")
    assert c.real_name != c.manifest_name

    d = summary.deps[Identifier("d")]
    assert d.source.is_registry
    assert d.source.as_registry().registry_url == "xd"
    assert len(d.versions) == 1
    assert d.versions[0] == Version(0, 1)
    assert d.real_name == Identifier("alias")
    assert d.manifest_name == Identifier("d")
    assert d.real_name != d.manifest_name

    e = summary.deps[Identifier("e")]
    assert e.source.is_git
    assert e.source.as_git().git_url == "git"
    assert e.source.as_git().branch == "branch"
    assert e.source.as_git().commit == "commit"
    assert e.source.as_git().tag is None
    assert not e.versions
    assert e.real_name == e.manifest_name


def test_fail_exclusive_git_fields(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1
    source:
      git_url: git
      tag: tag
      branch: branch
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert (
        str(excinfo.value)
        == "dependency `dependencies.a.source` is a git dependency, but contains mutually exclusive fields: `dependencies.a.source.tag`, `dependencies.a.source.branch`"
    )


def test_features(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

features:
  a: []
""")
    m = parse_manifest(x, GlobalContext.default())
    summary = m.summary
    assert len(summary.features) == 1
    assert summary.features[Identifier("a")] == []


def test_features_expansion(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

features:
  a: [b]
  b: [c, d]
  c: []
  d: []
""")
    m = parse_manifest(x, GlobalContext.default())
    summary = m.summary
    assert len(summary.features) == 4
    a = Identifier("a")
    b = Identifier("b")
    c = Identifier("c")
    d = Identifier("d")
    assert summary.features[a] == [b]
    assert summary.features[b] == [c, d]
    assert summary.features[c] == []
    assert summary.features[d] == []
    assert summary.features.expand_feature(a) == [a, b, c, d]
    assert summary.features.expand_feature(b) == [b, c, d]


def test_dep_features(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1
    features: [a, b]
""")

    m = parse_manifest(x, GlobalContext.default())
    summary = m.summary
    a = Identifier("a")
    b = Identifier("b")
    dep = summary.deps[a]
    assert [x.name for x in dep.features] == [a, b]
    assert list(dep.enabled_features([])) == [a, b]


def test_dep_features_with_conds(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1
    features:
      - a
      -
        b:
          package_features:
            - a
        c:
          system:
            - windows
""")

    m = parse_manifest(x, GlobalContext.default())
    summary = m.summary
    a = Identifier("a")
    b = Identifier("b")
    c = Identifier("c")
    dep = summary.deps[a]
    assert [x.name for x in dep.features] == [a, b, c]
    if sys.platform == "win32":
        assert list(dep.enabled_features([])) == [a, c]
        assert list(dep.enabled_features([a])) == [a, b, c]
    else:
        assert list(dep.enabled_features([])) == [a]
        assert list(dep.enabled_features([a])) == [a, b]


def test_empty_conditions_arch(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1
    conditions:
      arch: []
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert str(excinfo.value) == "`dependencies.a.conditions.arch` is an empty list"


def test_empty_conditions_system(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1
    conditions:
      system: []
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert str(excinfo.value) == "`dependencies.a.conditions.system` is an empty list"


def test_empty_conditions_features(tmp_path: Path):
    x = tmp_path / "x"
    x.touch()
    x.write_text("""
metadata:
  name: xd
  version: 0.1

dependencies:
  a:
    version: 0.1
    conditions:
      package_features: []
""")
    with pytest.raises(QuackPackError) as excinfo:
        parse_manifest(x, GlobalContext.default())
    assert str(excinfo.value) == "`dependencies.a.conditions.package_features` is an empty list"
