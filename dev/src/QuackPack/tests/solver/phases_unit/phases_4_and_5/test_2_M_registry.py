from pathlib import Path
from typing import Any, override
from unittest.mock import mock_open

import pytest

from quackpack.core.solver.gathering import GatheredInfo
from quackpack.core.solver.solving.build_rules_constructor import construct_build_rules
from quackpack.core.solver.solving.solver_engine import run_engine
from quackpack.core.solver.types.flag_type import FeatureId
from quackpack.core.solver.types.packages_by_id import PackagesById
from quackpack.core.solver.types.resolved_id import ResolvedIdGit, ResolvedIdLocal, ResolvedIdRegistry
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


def tests_2_medium_with_flags(tmp_path: Path):
    main_id = ResolvedIdLocal(local_path=Path.cwd())
    U_id = ResolvedIdRegistry(registry_url=DEFAULT_SERVER_URL, package_name=Identifier("U"))
    V_id = ResolvedIdRegistry(registry_url=DEFAULT_SERVER_URL, package_name=Identifier("V"))
    W_id = ResolvedIdRegistry(registry_url=DEFAULT_SERVER_URL, package_name=Identifier("W"))
    X_id = ResolvedIdRegistry(registry_url=DEFAULT_SERVER_URL, package_name=Identifier("X"))
    Y_id = ResolvedIdRegistry(registry_url=DEFAULT_SERVER_URL, package_name=Identifier("Y"))
    Z_id = ResolvedIdRegistry(registry_url=DEFAULT_SERVER_URL, package_name=Identifier("Z"))

    main_v1 = ResolvedPackageLocal(_id=main_id)
    U_v13 = ResolvedPackageRegistry(_id=U_id, _version=Version(1, 3))
    U_v27 = ResolvedPackageRegistry(_id=U_id, _version=Version(2, 7))
    U_v312 = ResolvedPackageRegistry(_id=U_id, _version=Version(3, 1, 2))
    V_v1 = ResolvedPackageRegistry(_id=V_id, _version=Version(1))
    V_v2 = ResolvedPackageRegistry(_id=V_id, _version=Version(2))
    W_v1 = ResolvedPackageRegistry(_id=W_id, _version=Version(1))
    X_v1 = ResolvedPackageRegistry(_id=X_id, _version=Version(1))
    X_v11 = ResolvedPackageRegistry(_id=X_id, _version=Version(1, 1))
    Y_v1 = ResolvedPackageRegistry(_id=Y_id, _version=Version(1))
    Y_v2 = ResolvedPackageRegistry(_id=Y_id, _version=Version(2))
    Y_v3 = ResolvedPackageRegistry(_id=Y_id, _version=Version(3))
    Y_v314 = ResolvedPackageRegistry(_id=Y_id, _version=Version(3, 1, 4))
    Y_v4 = ResolvedPackageRegistry(_id=Y_id, _version=Version(4))
    Y_v5 = ResolvedPackageRegistry(_id=Y_id, _version=Version(5))
    Z_v24 = ResolvedPackageRegistry(_id=Z_id, _version=Version(2, 4))
    Z_v263 = ResolvedPackageRegistry(_id=Z_id, _version=Version(2, 6, 3))

    summaries: dict[ResolvedPackage, Summary] = {}
    manifest_main_v1 = """
metadata:
    version: 1.0.0
    author: Main
    name: Main
    license: MIT

dependencies:
    Y:
        version: 2 or 3 or 4
        features: [{y: {project_features: [m1]}}]
    Z:
        version: 2.4
        features: [{z1: {project_features: [m2]}}]
    W:
        version: 1.0.0
    U:
        version: 3.1.2
    X:
        version: 1.0.0
        conditions:
            project_features: [m3]

features: {m1: [m1], m2: [m2], m3: [m3]}"""
    m = mock_open(read_data=manifest_main_v1)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[main_v1] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_U_v13 = """
metadata:
    version: 1.3.0
    author: U
    name: U
    license: MIT"""
    m = mock_open(read_data=manifest_U_v13)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[U_v13] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_U_v27 = """
metadata:
    version: 2.7.0
    author: U
    name: U
    license: MIT"""
    m = mock_open(read_data=manifest_U_v27)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[U_v27] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_U_v312 = """
metadata:
    version: 3.1.2
    author: U
    name: U
    license: MIT

dependencies:
    Y:
        version: 1.0.0"""
    m = mock_open(read_data=manifest_U_v312)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[U_v312] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_V_v1 = """
metadata:
    version: 1.0.0
    author: V
    name: V
    license: MIT"""
    m = mock_open(read_data=manifest_V_v1)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[V_v1] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_V_v2 = """
metadata:
    version: 2.0.0
    author: V
    name: V
    license: MIT"""
    m = mock_open(read_data=manifest_V_v2)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[V_v2] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_W_v1 = """
metadata:
    version: 1.0.0
    author: W
    name: W
    license: MIT

dependencies:
    Y:
        version: 3.1.4
    Z:
        version: 2.6
        features: [z2]"""
    m = mock_open(read_data=manifest_W_v1)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[W_v1] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_X_v1 = """
metadata:
    version: 1.0.0
    author: X
    name: X
    license: MIT

dependencies:
    Y:
        version: 5.0.0
"""
    m = mock_open(read_data=manifest_X_v1)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[X_v1] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_X_v11 = """
metadata:
    version: 1.1.0
    author: X
    name: X
    license: MIT
"""
    m = mock_open(read_data=manifest_X_v11)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[X_v11] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_Y_v1 = """
metadata:
    version: 1.0.0
    author: Y
    name: Y
    license: MIT"""
    m = mock_open(read_data=manifest_Y_v1)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[Y_v1] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_Y_v2 = """
metadata:
    version: 2.0.0
    author: Y
    name: Y
    license: MIT

features: {y: [y]}
"""
    m = mock_open(read_data=manifest_Y_v2)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[Y_v2] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_Y_v3 = """
metadata:
    version: 3.0.0
    author: Y
    name: Y
    license: MIT

features: {y: [y]}
"""
    m = mock_open(read_data=manifest_Y_v3)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[Y_v3] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_Y_v314 = """
metadata:
    version: 3.1.4
    author: Y
    name: Y
    license: MIT

features: {y: [y]}"""
    m = mock_open(read_data=manifest_Y_v314)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[Y_v314] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_Y_v4 = """
metadata:
    version: 4.0.0
    author: Y
    name: Y
    license: MIT

features: {y: [y]}"""
    m = mock_open(read_data=manifest_Y_v4)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[Y_v4] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_Y_v5 = """
metadata:
    version: 5.0.0
    author: Y
    name: Y
    license: MIT

features: {y: [y]}"""
    m = mock_open(read_data=manifest_Y_v5)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[Y_v5] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_Z_v24 = """
metadata:
    version: 2.4.0
    author: Z
    name: Z
    license: MIT

dependencies:
    U:
        version: 1.3
    V:
        version: 1.0.0
        conditions:
            project_features: [z1]

features: {z1: [z1], z2: [z2]}"""
    m = mock_open(read_data=manifest_Z_v24)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[Z_v24] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    manifest_Z_v263 = """
metadata:
    version: 2.6.3
    author: Z
    name: Z
    license: MIT

dependencies:
    U:
        version: 2.7
    V:
        version: 2.0.0
        conditions:
            project_features: [z1]

features: {z1: [z1], z2: [z2]}"""
    m = mock_open(read_data=manifest_Z_v263)
    tmp_file = tmp_path / "x"
    tmp_file.touch()
    with pytest.MonkeyPatch.context() as mpatch:
        mpatch.setattr("builtins.open", m)
        summaries[Z_v263] = parse_manifest(tmp_file, FakeGlobalContext.default()).summary

    packages_by_id: PackagesById = PackagesById()
    packages_by_id[main_id].add(main_v1)
    packages_by_id[U_id].add(U_v13)
    packages_by_id[U_id].add(U_v27)
    packages_by_id[U_id].add(U_v312)
    packages_by_id[V_id].add(V_v1)
    packages_by_id[V_id].add(V_v2)
    packages_by_id[W_id].add(W_v1)
    packages_by_id[X_id].add(X_v1)
    packages_by_id[X_id].add(X_v11)
    packages_by_id[Y_id].add(Y_v1)
    packages_by_id[Y_id].add(Y_v2)
    packages_by_id[Y_id].add(Y_v3)
    packages_by_id[Y_id].add(Y_v314)
    packages_by_id[Y_id].add(Y_v4)
    packages_by_id[Y_id].add(Y_v5)
    packages_by_id[Z_id].add(Z_v24)
    packages_by_id[Z_id].add(Z_v263)

    possible_flags: dict[ResolvedPackage, set[FeatureId]] = {}
    possible_flags[main_v1] = {Identifier("m1"), Identifier("m2"), Identifier("m3")}
    possible_flags[U_v13] = set()
    possible_flags[U_v27] = set()
    possible_flags[U_v312] = set()
    possible_flags[V_v1] = set()
    possible_flags[V_v2] = set()
    possible_flags[W_v1] = set()
    possible_flags[X_v1] = set()
    possible_flags[X_v11] = set()
    possible_flags[Y_v1] = set()
    possible_flags[Y_v2] = {Identifier("y")}
    possible_flags[Y_v3] = {Identifier("y")}
    possible_flags[Y_v314] = {Identifier("y")}
    possible_flags[Y_v4] = {Identifier("y")}
    possible_flags[Y_v5] = set()
    possible_flags[Z_v24] = {Identifier("z1")}
    possible_flags[Z_v263] = {Identifier("z1"), Identifier("z2")}

    registry_resolvents: dict[UnresolvedIdRegistry, ResolvedIdRegistry] = {
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("U")): U_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("V")): V_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("W")): W_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("X")): X_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("Y")): Y_id,
        UnresolvedIdRegistry(DEFAULT_SERVER_URL, Identifier("Z")): Z_id,
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

    gathered = GatheredInfo(
        summaries=summaries,
        packages_by_id=packages_by_id,
        possible_features=possible_flags,
        id_resolvents=id_resolvents,
    )

    solution = run_engine(
        data=gathered, root_projects=[(main_v1, {Identifier("m1"), Identifier("m2"), Identifier("m3")})]
    )
    build_rules = construct_build_rules(summaries=summaries, solution=solution, id_resolvents=id_resolvents)

    assert set(build_rules.flags_to_install[main_v1]) == {
        Identifier("m1"),
        Identifier("m2"),
        Identifier("m3"),
    }
    assert build_rules.flags_to_install[U_v27] == []
    assert build_rules.flags_to_install[U_v312] == []
    assert build_rules.flags_to_install[V_v2] == []
    assert build_rules.flags_to_install[W_v1] == []
    assert build_rules.flags_to_install[X_v11] == []
    assert build_rules.flags_to_install[Y_v1] == []
    assert build_rules.flags_to_install[Y_v314] == [Identifier("y")]
    assert set(build_rules.flags_to_install[Z_v263]) == {Identifier("z1"), Identifier("z2")}
    assert {U_v13, V_v1, X_v1, Y_v2, Y_v3, Y_v4, Y_v5, Z_v24}.intersection(
        set(build_rules.flags_to_install.keys())
    ) == set()

    assert build_rules.instructions[main_v1] == {
        Identifier("U"): U_v312,
        Identifier("W"): W_v1,
        Identifier("X"): X_v11,
        Identifier("Y"): Y_v314,
        Identifier("Z"): Z_v263,
    }
    assert build_rules.instructions[U_v27] == {}
    assert build_rules.instructions[U_v312] == {Identifier("Y"): Y_v1}
    assert build_rules.instructions[V_v2] == {}
    assert build_rules.instructions[W_v1] == {Identifier("Y"): Y_v314, Identifier("Z"): Z_v263}
    assert build_rules.instructions[X_v11] == {}
    assert build_rules.instructions[Y_v1] == {}
    assert build_rules.instructions[Y_v314] == {}
    assert build_rules.instructions[Z_v263] == {Identifier("V"): V_v2, Identifier("U"): U_v27}
    assert {U_v13, V_v1, X_v1, Y_v2, Y_v3, Y_v4, Y_v5, Z_v24}.intersection(
        set(build_rules.instructions.keys())
    ) == set()
