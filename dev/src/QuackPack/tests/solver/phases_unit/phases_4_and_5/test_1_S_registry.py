# Assets for test 1
from pathlib import Path
from typing import Any, override
from unittest.mock import mock_open

import pytest

from quackpack.core.solver.gathering import GatheredInfo
from quackpack.core.solver.solving.build_rules_constructor import construct_build_rules
from quackpack.core.solver.solving.solver_engine import run_engine
from quackpack.core.solver.types.flag_type import FeatureId
from quackpack.core.solver.types.packages_by_id import PackagesById
from quackpack.core.solver.types.resolved_id import (
    ResolvedIdGit,
    ResolvedIdLocal,
    ResolvedIdRegistry,
)
from quackpack.core.solver.types.resolved_package import (
    ResolvedPackage,
    ResolvedPackageLocal,
    ResolvedPackageRegistry,
)
from quackpack.core.solver.types.unresolved_id import (
    IdResolvents,
    UnresolvedIdGit,
    UnresolvedIdLocal,
    UnresolvedIdRegistry,
)
from quackpack.core.types.manifest.parse import parse_manifest
from quackpack.core.types.manifest.summary import Summary
from quackpack.util.global_context import GlobalContext
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version

DEFAULT_SERVER_URL = "https://www.youtube.com/watch?v=dQw4w9WgXcQ&ab_channel=RickAstley"


class FakeGlobalContext(GlobalContext):
    def __init__(self, *args: Any, **kwargs: Any):
        _ = args
        _ = kwargs

    @override
    def registry_url(self) -> str:
        return DEFAULT_SERVER_URL


def test_1_small_with_flags(tmp_path: Path):
    main_id = ResolvedIdLocal(local_path=Path.cwd())
    pack1_id = ResolvedIdRegistry(
        registry_url=DEFAULT_SERVER_URL, package_name=Identifier("pack1")
    )
    pack2_id = ResolvedIdRegistry(
        registry_url=DEFAULT_SERVER_URL, package_name=Identifier("pack2")
    )
    pack3_id = ResolvedIdRegistry(
        registry_url=DEFAULT_SERVER_URL, package_name=Identifier("pack3")
    )
    pack4_id = ResolvedIdRegistry(
        registry_url=DEFAULT_SERVER_URL, package_name=Identifier("pack4")
    )
    pack5_id = ResolvedIdRegistry(
        registry_url=DEFAULT_SERVER_URL, package_name=Identifier("pack5")
    )

    main_v1 = ResolvedPackageLocal(_id=main_id)
    pack1_v1 = ResolvedPackageRegistry(_id=pack1_id, _version=Version(1))
    pack1_v2 = ResolvedPackageRegistry(_id=pack1_id, _version=Version(2))
    pack2_v1 = ResolvedPackageRegistry(_id=pack2_id, _version=Version(1))
    pack3_v1 = ResolvedPackageRegistry(_id=pack3_id, _version=Version(1))
    pack3_v2 = ResolvedPackageRegistry(_id=pack3_id, _version=Version(2))
    pack4_v1 = ResolvedPackageRegistry(_id=pack4_id, _version=Version(1))
    pack4_v2 = ResolvedPackageRegistry(_id=pack4_id, _version=Version(2))
    pack5_v1 = ResolvedPackageRegistry(_id=pack5_id, _version=Version(1))

    registry_resolvents: dict[UnresolvedIdRegistry, ResolvedIdRegistry] = {
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("pack1")): pack1_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("pack2")): pack2_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("pack3")): pack3_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("pack4")): pack4_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("pack5")): pack5_id,
    }
    local_resolvents: dict[UnresolvedIdLocal, ResolvedIdLocal] = {
        UnresolvedIdLocal(local_path=Path.cwd()): main_id
    }
    git_resolvents: dict[UnresolvedIdGit, ResolvedIdGit] = {}
    id_resolvents = IdResolvents(
        registry_resolvents=registry_resolvents,
        git_resolvents=git_resolvents,
        local_resolvents=local_resolvents,
    )

    summaries: dict[ResolvedPackage, Summary] = {}
    manifest_main_v1 = """
metadata:
  name: main
  version: 1
dependencies:
  pack1:
    version: 1 or 2
    features: [A]
  pack2:
    version: 1
    features: [B]
  pack5:
    version: 1
    conditions:
      project_features: [C]
"""
    m = mock_open(read_data=manifest_main_v1)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[main_v1] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    manifest_pack1_v1 = """
metadata:
  name: pack1
  version: 1
dependencies:
  pack3:
    version: 1
"""
    m = mock_open(read_data=manifest_pack1_v1)
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[pack1_v1] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    manifest_pack1_v2 = """
metadata:
  name: pack1
  version: 2
dependencies:
  pack3:
    version: 2
"""
    m = mock_open(read_data=manifest_pack1_v2)
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[pack1_v2] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    manifest_pack2_v1 = """
metadata:
  name: pack2
  version: 1
dependencies:
  pack4:
    version: 1 or 2
    conditions:
      project_features: [B]
    features: [D]
"""
    m = mock_open(read_data=manifest_pack2_v1)
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[pack2_v1] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    manifest_pack3_v1 = """
metadata:
  name: pack3
  version: 1
"""
    m = mock_open(read_data=manifest_pack3_v1)
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[pack3_v1] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    manifest_pack3_v2 = """
metadata:
  name: pack3
  version: 1
"""
    m = mock_open(read_data=manifest_pack3_v2)
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[pack3_v2] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    manifest_pack4_v1 = """
metadata:
  name: pack4
  version: 1
"""
    m = mock_open(read_data=manifest_pack4_v1)
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[pack4_v1] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    manifest_pack4_v2 = """
metadata:
  name: pack4
  version: 1
"""
    m = mock_open(read_data=manifest_pack4_v2)
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[pack4_v2] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    manifest_pack5_v1 = """
metadata:
  name: pack5
  version: 1
"""
    m = mock_open(read_data=manifest_pack5_v1)
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[pack5_v1] = parse_manifest(
            tmp_file, FakeGlobalContext.default()
        ).summary

    packages_by_id: PackagesById = PackagesById()
    packages_by_id[main_id].add(main_v1)
    packages_by_id[pack1_id].add(pack1_v1)
    packages_by_id[pack1_id].add(pack1_v2)
    packages_by_id[pack2_id].add(pack2_v1)
    packages_by_id[pack3_id].add(pack3_v1)
    packages_by_id[pack3_id].add(pack3_v2)
    packages_by_id[pack4_id].add(pack4_v1)
    packages_by_id[pack4_id].add(pack4_v2)
    packages_by_id[pack5_id].add(pack5_v1)

    possible_flags: dict[ResolvedPackage, set[FeatureId]] = {}
    possible_flags[main_v1] = {Identifier("C")}
    possible_flags[pack1_v1] = {Identifier("A")}
    possible_flags[pack1_v2] = {Identifier("A")}
    possible_flags[pack2_v1] = {Identifier("B")}
    possible_flags[pack3_v1] = set()
    possible_flags[pack3_v2] = set()
    possible_flags[pack4_v1] = {Identifier("D")}
    possible_flags[pack4_v2] = {Identifier("D")}
    possible_flags[pack5_v1] = set()

    gathered = GatheredInfo(
        summaries=summaries,
        packages_by_id=packages_by_id,
        possible_features=possible_flags,
        id_resolvents=id_resolvents,
    )

    solution = run_engine(data=gathered, root_projects=[(main_v1, {Identifier("C")})])

    build_rules = construct_build_rules(
        summaries=summaries, solution=solution, id_resolvents=id_resolvents
    )
    assert build_rules.flags_to_install[main_v1] == [Identifier("C")]
    assert build_rules.flags_to_install[pack5_v1] == []
    assert build_rules.instructions[pack5_v1] == {}
    assert build_rules.flags_to_install[pack2_v1] == [Identifier("B")]
    assert (pack1_v1 in build_rules.flags_to_install) ^ (
        pack1_v2 in build_rules.flags_to_install
    )
    if pack1_v1 in build_rules.flags_to_install:
        assert build_rules.instructions[main_v1] == {
            Identifier("pack1"): pack1_v1,
            Identifier("pack2"): pack2_v1,
            Identifier("pack5"): pack5_v1,
        }
        assert build_rules.instructions[pack1_v1] == {Identifier("pack3"): pack3_v1}
        assert build_rules.instructions[pack3_v1] == {}
        assert pack3_v2 not in build_rules.instructions
        assert pack1_v2 not in build_rules.instructions
        assert build_rules.flags_to_install[pack1_v1] == [Identifier("A")]
        assert build_rules.flags_to_install[pack3_v1] == []
        assert pack3_v2 not in build_rules.flags_to_install
    else:
        assert build_rules.instructions[main_v1] == {
            Identifier("pack1"): pack1_v2,
            Identifier("pack2"): pack2_v1,
            Identifier("pack5"): pack5_v1,
        }
        assert build_rules.instructions[pack1_v2] == {Identifier("pack3"): pack3_v2}
        assert build_rules.instructions[pack3_v2] == {}
        assert pack3_v1 not in build_rules.instructions
        assert pack1_v1 not in build_rules.instructions
        assert build_rules.flags_to_install[pack1_v2] == [Identifier("A")]
        assert build_rules.flags_to_install[pack3_v2] == []
        assert pack3_v1 not in build_rules.flags_to_install
    assert (pack4_v1 in build_rules.flags_to_install) ^ (
        pack4_v2 in build_rules.flags_to_install
    )
    if pack4_v1 in build_rules.flags_to_install:
        assert build_rules.instructions[pack2_v1] == {Identifier("pack4"): pack4_v1}
        assert build_rules.instructions[pack4_v1] == {}
        assert pack4_v2 not in build_rules.instructions
        assert build_rules.flags_to_install[pack4_v1] == [Identifier("D")]
    else:
        assert build_rules.instructions[pack2_v1] == {Identifier("pack4"): pack4_v2}
        assert build_rules.instructions[pack4_v2] == {}
        assert pack4_v1 not in build_rules.instructions
        assert build_rules.flags_to_install[pack4_v2] == [Identifier("D")]
