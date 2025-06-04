from pathlib import Path

import pytest
from pydantic import AnyUrl

from quackpack.config.project import (
    Dependencies,
    DependencyConditions,
    DependencyEntry,
    GitEntry,
    LocalEntry,
    Manifest,
    Metadata,
    VersionList,
)
from quackpack.solver.dependency_exploring import DependencyConstructor
from quackpack.solver.universal_names import UniversalName
from quackpack.solver.util import NoFlags, SolverPackage, SolverPackageWithFlag
from quackpack.util.pkgid import Identifier
from quackpack.util.version import Version

# TODO simpler tests (2 packages), 1 even larger test
# TODO something with git
# Inputs for test 3
main_uni_name3 = UniversalName.server_uni_name(Identifier("MAIN"), AnyUrl("http://www.google.com"))
pack1_uni_name3 = UniversalName.server_uni_name(Identifier("package1"), AnyUrl("http://www.google.com"))
pack2_uni_name3 = UniversalName.server_uni_name(Identifier("package2"), AnyUrl("http://www.google.com"))
pack3_uni_name3 = UniversalName.server_uni_name(Identifier("package3"), AnyUrl("http://www.google.com"))

main_v01_3 = SolverPackage(name=main_uni_name3, version=Version(0, 1))
pack1_v1_3 = SolverPackage(name=pack1_uni_name3, version=Version(1))
pack2_v2_3 = SolverPackage(name=pack2_uni_name3, version=Version(2))
pack2_v3_3 = SolverPackage(name=pack2_uni_name3, version=Version(3))
pack3_v4_3 = SolverPackage(name=pack3_uni_name3, version=Version(4))
pack3_v5_3 = SolverPackage(name=pack3_uni_name3, version=Version(5))

main_v01_N_3 = SolverPackageWithFlag(package=main_v01_3, flag=NoFlags.NO_FLAGS)
pack1_v1_N_3 = SolverPackageWithFlag(package=pack1_v1_3, flag=NoFlags.NO_FLAGS)
pack1_v1_A_3 = SolverPackageWithFlag(package=pack1_v1_3, flag="A")
pack1_v1_B_3 = SolverPackageWithFlag(package=pack1_v1_3, flag="B")
pack2_v2_N_3 = SolverPackageWithFlag(package=pack2_v2_3, flag=NoFlags.NO_FLAGS)
pack2_v2_X_3 = SolverPackageWithFlag(package=pack2_v2_3, flag="X")
pack2_v3_N_3 = SolverPackageWithFlag(package=pack2_v3_3, flag=NoFlags.NO_FLAGS)
pack2_v3_X_3 = SolverPackageWithFlag(package=pack2_v3_3, flag="X")
pack3_v4_N_3 = SolverPackageWithFlag(package=pack3_v4_3, flag=NoFlags.NO_FLAGS)
pack3_v5_N_3 = SolverPackageWithFlag(package=pack3_v5_3, flag=NoFlags.NO_FLAGS)
pack3_v5_Y_3 = SolverPackageWithFlag(package=pack3_v5_3, flag="Y")

gathered_configs3: dict[SolverPackage, Manifest] = {}
gathered_configs3[main_v01_3] = Manifest(
    metadata=Metadata(author="Grzegorz Brzeczyszczykiewicz", version=Version(0, 1), name=Identifier("MAIN")),
    dependencies=Dependencies(
        root={
            Identifier("package1"): DependencyEntry(
                version=VersionList(root=[Version(1)]), flags=[Identifier("A"), Identifier("B")]
            )
        }
    ),
)
gathered_configs3[pack1_v1_3] = Manifest(
    metadata=Metadata(author="Patryk Rogalski", version=Version(1), name=Identifier("package1")),
    dependencies=Dependencies(
        root={
            Identifier("package2"): DependencyEntry(
                version=VersionList(root=[Version(2), Version(3)]),
                conditions=DependencyConditions(project_flags=[Identifier("A")]),
                flags=[Identifier("X")],
            ),
            Identifier("package3"): DependencyEntry(version=VersionList(root=[Version(4)])),
        }
    ),
)
gathered_configs3[pack2_v2_3] = Manifest(
    metadata=Metadata(author="Patryk Rogalski", version=Version(2), name=Identifier("package2")),
    dependencies=Dependencies(
        root={
            Identifier("package3"): DependencyEntry(
                version=VersionList(root=[Version(5)]),
                conditions=DependencyConditions(project_flags=[Identifier("X")]),
                flags=[Identifier("Y")],
            )
        }
    ),
)
gathered_configs3[pack2_v3_3] = Manifest(
    metadata=Metadata(author="Patryk Rogalski", version=Version(3), name=Identifier("package2"))
)
gathered_configs3[pack3_v4_3] = Manifest(
    metadata=Metadata(author="Patryk Rogalski", version=Version(4), name=Identifier("package3"))
)
gathered_configs3[pack3_v5_3] = Manifest(
    metadata=Metadata(author="Patryk Rogalski", version=Version(5), name=Identifier("package3"))
)

versions_by_universal_name3: dict[UniversalName, list[Version | GitEntry | LocalEntry]] = {}
versions_by_universal_name3[main_uni_name3] = [Version(0, 1)]
versions_by_universal_name3[pack1_uni_name3] = [Version(1)]
versions_by_universal_name3[pack2_uni_name3] = [Version(2), Version(3)]
versions_by_universal_name3[pack3_uni_name3] = [Version(4), Version(5)]

possible_flags3: dict[SolverPackage, set[NoFlags | str]] = {}

possible_flags3[main_v01_3] = {NoFlags.NO_FLAGS}
possible_flags3[pack1_v1_3] = {NoFlags.NO_FLAGS, "A", "B"}
possible_flags3[pack2_v2_3] = {NoFlags.NO_FLAGS, "X"}
possible_flags3[pack2_v3_3] = {NoFlags.NO_FLAGS, "X"}
possible_flags3[pack3_v4_3] = {NoFlags.NO_FLAGS}
possible_flags3[pack3_v5_3] = {NoFlags.NO_FLAGS, "Y"}

# Inputs for test 4
pack1_uni_name4 = UniversalName.local_uni_name(Path.cwd())
pack2_uni_name4 = UniversalName.local_uni_name(Path.cwd().parent)
pack3_uni_name4 = UniversalName.local_uni_name(Path.cwd().parent.parent)

pack1_v1_4 = SolverPackage(name=pack1_uni_name4, version=LocalEntry(path=Path.cwd()))
pack2_v1_4 = SolverPackage(name=pack2_uni_name4, version=LocalEntry(path=Path.cwd().parent))
pack3_v1_4 = SolverPackage(name=pack3_uni_name4, version=LocalEntry(path=Path.cwd().parent.parent))

pack1_v1_N_4 = SolverPackageWithFlag(package=pack1_v1_4, flag=NoFlags.NO_FLAGS)
pack2_v1_N_4 = SolverPackageWithFlag(package=pack2_v1_4, flag=NoFlags.NO_FLAGS)
pack3_v1_N_4 = SolverPackageWithFlag(package=pack3_v1_4, flag=NoFlags.NO_FLAGS)

gathered_configs4: dict[SolverPackage, Manifest] = {}
gathered_configs4[pack1_v1_4] = Manifest(
    metadata=Metadata(author="Patryk Rogalski", version=Version(1), name=Identifier("pack1")),
    dependencies=Dependencies({
        Identifier("pack2"): DependencyEntry(version=LocalEntry(path=Path.cwd().parent))
    }),
)
gathered_configs4[pack2_v1_4] = Manifest(
    metadata=Metadata(author="Patryk Rogalski", version=Version(1), name=Identifier("pack2")),
    dependencies=Dependencies({
        Identifier("pack3"): DependencyEntry(version=LocalEntry(path=Path.cwd().parent.parent))
    }),
)
gathered_configs4[pack3_v1_4] = Manifest(
    metadata=Metadata(author="Patryk Rogalski", version=Version(1), name=Identifier("pack3")),
    dependencies=Dependencies({Identifier("pack1"): DependencyEntry(version=LocalEntry(path=Path.cwd()))}),
)

versions_by_universal_name4: dict[UniversalName, list[Version | GitEntry | LocalEntry]] = {}
versions_by_universal_name4[pack1_uni_name4] = [LocalEntry(path=Path.cwd())]
versions_by_universal_name4[pack2_uni_name4] = [LocalEntry(path=Path.cwd().parent)]
versions_by_universal_name4[pack3_uni_name4] = [LocalEntry(path=Path.cwd().parent.parent)]

possible_flags4: dict[SolverPackage, set[NoFlags | str]] = {}
possible_flags4[pack1_v1_4] = {NoFlags.NO_FLAGS}
possible_flags4[pack2_v1_4] = {NoFlags.NO_FLAGS}
possible_flags4[pack3_v1_4] = {NoFlags.NO_FLAGS}


class TestDependencyConstructing:
    @pytest.mark.skip(reason="Broken solver")
    def test_3_middle_size_example(self):
        constructor = DependencyConstructor(
            gathered_configs=gathered_configs3,
            versions_by_universal_name=versions_by_universal_name3,
            possible_flags=possible_flags3,
        )
        constructor.explore_dependencies()

        # Test parents
        assert main_v01_N_3 not in constructor.parents
        assert constructor.parents[pack1_v1_N_3] == [main_v01_N_3]
        assert constructor.parents[pack1_v1_A_3] == [main_v01_N_3]
        assert constructor.parents[pack1_v1_B_3] == [main_v01_N_3]
        assert constructor.parents[pack2_v2_N_3] == [pack1_v1_A_3]
        assert constructor.parents[pack2_v2_X_3] == [pack1_v1_A_3]
        assert constructor.parents[pack2_v3_N_3] == [pack1_v1_A_3]
        assert constructor.parents[pack2_v3_X_3] == [pack1_v1_A_3]
        assert set(constructor.parents[pack3_v4_N_3]) == {pack1_v1_N_3, pack1_v1_A_3, pack1_v1_B_3}
        assert constructor.parents[pack3_v5_N_3] == [pack2_v2_X_3]
        assert constructor.parents[pack3_v5_Y_3] == [pack2_v2_X_3]

        # Test n_children
        assert constructor.n_children[main_v01_N_3] == {
            pack1_uni_name3: {NoFlags.NO_FLAGS: 1, "A": 1, "B": 1}
        }
        assert constructor.n_children[pack1_v1_N_3] == {pack3_uni_name3: {NoFlags.NO_FLAGS: 1}}
        assert constructor.n_children[pack1_v1_A_3] == {
            pack2_uni_name3: {NoFlags.NO_FLAGS: 2, "X": 2},
            pack3_uni_name3: {NoFlags.NO_FLAGS: 1},
        }
        assert constructor.n_children[pack1_v1_B_3] == {pack3_uni_name3: {NoFlags.NO_FLAGS: 1}}
        assert constructor.n_children[pack2_v2_N_3] == {}
        assert constructor.n_children[pack2_v2_X_3] == {pack3_uni_name3: {NoFlags.NO_FLAGS: 1, "Y": 1}}
        assert constructor.n_children[pack2_v3_N_3] == {}
        assert constructor.n_children[pack2_v3_X_3] == {}
        assert constructor.n_children[pack3_v4_N_3] == {}
        assert constructor.n_children[pack3_v5_N_3] == {}
        assert constructor.n_children[pack3_v5_Y_3] == {}

    def test_4_cycle_of_locals(self):
        constructor = DependencyConstructor(
            gathered_configs=gathered_configs4,
            versions_by_universal_name=versions_by_universal_name4,
            possible_flags=possible_flags4,
        )
        constructor.explore_dependencies()

        assert constructor.parents[pack1_v1_N_4] == [pack3_v1_N_4]
        assert constructor.parents[pack2_v1_N_4] == [pack1_v1_N_4]
        assert constructor.parents[pack3_v1_N_4] == [pack2_v1_N_4]

        assert constructor.n_children[pack1_v1_N_4] == {pack2_uni_name4: {NoFlags.NO_FLAGS: 1}}
        assert constructor.n_children[pack2_v1_N_4] == {pack3_uni_name4: {NoFlags.NO_FLAGS: 1}}
        assert constructor.n_children[pack3_v1_N_4] == {pack1_uni_name4: {NoFlags.NO_FLAGS: 1}}
