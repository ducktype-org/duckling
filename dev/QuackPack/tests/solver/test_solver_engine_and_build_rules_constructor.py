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
from quackpack.solver.build_rules_constructor import BuildRulesConstructor
from quackpack.solver.solver_engine import SolverEngine
from quackpack.solver.universal_names import UniversalName
from quackpack.solver.util import NoFlags, SolverPackage, SolverPackageWithFlag
from quackpack.util.pkgid import Identifier
from quackpack.util.version import Version

# Assets for test 1

main_uni_name1 = UniversalName.local_uni_name(Path.cwd())
pack1_uni_name1 = UniversalName.server_uni_name(Identifier("pack1"), AnyUrl("http://www.google.com"))
pack2_uni_name1 = UniversalName.server_uni_name(Identifier("pack2"), AnyUrl("http://www.google.com"))
pack3_uni_name1 = UniversalName.server_uni_name(Identifier("pack3"), AnyUrl("http://www.google.com"))
pack4_uni_name1 = UniversalName.server_uni_name(Identifier("pack4"), AnyUrl("http://www.google.com"))
pack5_uni_name1 = UniversalName.server_uni_name(Identifier("pack5"), AnyUrl("http://www.google.com"))

main_v1_1 = SolverPackage(name=main_uni_name1, version=LocalEntry(path=Path.cwd()))
pack1_v1_1 = SolverPackage(name=pack1_uni_name1, version=Version(1))
pack1_v2_1 = SolverPackage(name=pack1_uni_name1, version=Version(2))
pack2_v1_1 = SolverPackage(name=pack2_uni_name1, version=Version(1))
pack3_v1_1 = SolverPackage(name=pack3_uni_name1, version=Version(1))
pack3_v2_1 = SolverPackage(name=pack3_uni_name1, version=Version(2))
pack4_v1_1 = SolverPackage(name=pack4_uni_name1, version=Version(1))
pack4_v2_1 = SolverPackage(name=pack4_uni_name1, version=Version(2))
pack5_v1_1 = SolverPackage(name=pack5_uni_name1, version=Version(1))

main_v1_N1 = SolverPackageWithFlag(package=main_v1_1, flag=NoFlags.NO_FLAGS)
main_v1_C1 = SolverPackageWithFlag(package=main_v1_1, flag="C")

pack1_v1_N1 = SolverPackageWithFlag(package=pack1_v1_1, flag=NoFlags.NO_FLAGS)
pack1_v1_A1 = SolverPackageWithFlag(package=pack1_v1_1, flag="A")
pack1_v2_N1 = SolverPackageWithFlag(package=pack1_v2_1, flag=NoFlags.NO_FLAGS)
pack1_v2_A1 = SolverPackageWithFlag(package=pack1_v2_1, flag="A")

pack2_v1_N1 = SolverPackageWithFlag(package=pack2_v1_1, flag=NoFlags.NO_FLAGS)
pack2_v1_B1 = SolverPackageWithFlag(package=pack2_v1_1, flag="B")

pack3_v1_N1 = SolverPackageWithFlag(package=pack3_v1_1, flag=NoFlags.NO_FLAGS)
pack3_v2_N1 = SolverPackageWithFlag(package=pack3_v2_1, flag=NoFlags.NO_FLAGS)

pack4_v1_N1 = SolverPackageWithFlag(package=pack4_v1_1, flag=NoFlags.NO_FLAGS)
pack4_v1_D1 = SolverPackageWithFlag(package=pack4_v1_1, flag="D")
pack4_v2_N1 = SolverPackageWithFlag(package=pack4_v2_1, flag=NoFlags.NO_FLAGS)
pack4_v2_D1 = SolverPackageWithFlag(package=pack4_v2_1, flag="D")

pack5_v1_N1 = SolverPackageWithFlag(package=pack5_v1_1, flag=NoFlags.NO_FLAGS)

U_main1 = {
    main_v1_N1,
    main_v1_C1,
    pack1_v1_N1,
    pack1_v1_A1,
    pack1_v2_N1,
    pack1_v2_A1,
    pack2_v1_N1,
    pack2_v1_B1,
    pack3_v1_N1,
    pack3_v2_N1,
    pack4_v1_N1,
    pack4_v1_D1,
    pack4_v2_N1,
    pack4_v2_D1,
    pack5_v1_N1,
}

gathered_configs1: dict[SolverPackage, Manifest] = {}
gathered_configs1[main_v1_1] = Manifest(
    metadata=Metadata(name=Identifier("main"), version=Version(1)),
    dependencies=Dependencies({
        Identifier("pack1"): DependencyEntry(
            version=VersionList([Version(1), Version(2)]), flags=[Identifier("A")]
        ),
        Identifier("pack2"): DependencyEntry(version=VersionList([Version(1)]), flags=[Identifier("B")]),
        Identifier("pack5"): DependencyEntry(
            version=VersionList([Version(1)]),
            conditions=DependencyConditions(project_flags=[Identifier("C")]),
        ),
    }),
)
gathered_configs1[pack1_v1_1] = Manifest(
    metadata=Metadata(name=Identifier("pack1"), version=Version(1)),
    dependencies=Dependencies({Identifier("pack3"): DependencyEntry(version=VersionList([Version(1)]))}),
)
gathered_configs1[pack1_v2_1] = Manifest(
    metadata=Metadata(name=Identifier("pack1"), version=Version(2)),
    dependencies=Dependencies({Identifier("pack3"): DependencyEntry(version=VersionList([Version(2)]))}),
)
gathered_configs1[pack2_v1_1] = Manifest(
    metadata=Metadata(name=Identifier("pack2"), version=Version(1)),
    dependencies=Dependencies({
        Identifier("pack4"): DependencyEntry(
            version=VersionList([Version(1), Version(2)]),
            conditions=DependencyConditions(project_flags=[Identifier("B")]),
            flags=[Identifier("D")],
        )
    }),
)
gathered_configs1[pack3_v1_1] = Manifest(metadata=Metadata(name=Identifier("pack3"), version=Version(1)))
gathered_configs1[pack3_v2_1] = Manifest(metadata=Metadata(name=Identifier("pack3"), version=Version(2)))
gathered_configs1[pack4_v1_1] = Manifest(metadata=Metadata(name=Identifier("pack4"), version=Version(1)))
gathered_configs1[pack4_v2_1] = Manifest(metadata=Metadata(name=Identifier("pack4"), version=Version(2)))
gathered_configs1[pack5_v1_1] = Manifest(metadata=Metadata(name=Identifier("pack5"), version=Version(1)))

versions_by_universal_name1: dict[UniversalName, list[Version | GitEntry | LocalEntry]] = {}
versions_by_universal_name1[main_uni_name1] = [LocalEntry(path=Path.cwd())]
versions_by_universal_name1[pack1_uni_name1] = [Version(1), Version(2)]
versions_by_universal_name1[pack2_uni_name1] = [Version(1)]
versions_by_universal_name1[pack3_uni_name1] = [Version(1), Version(2)]
versions_by_universal_name1[pack4_uni_name1] = [Version(1), Version(2)]
versions_by_universal_name1[pack5_uni_name1] = [Version(1)]

possible_flags1: dict[SolverPackage, set[NoFlags | str]] = {}
possible_flags1[main_v1_1] = {NoFlags.NO_FLAGS, "C"}
possible_flags1[pack1_v1_1] = {NoFlags.NO_FLAGS, "A"}
possible_flags1[pack1_v2_1] = {NoFlags.NO_FLAGS, "A"}
possible_flags1[pack2_v1_1] = {NoFlags.NO_FLAGS, "B"}
possible_flags1[pack3_v1_1] = {NoFlags.NO_FLAGS}
possible_flags1[pack3_v2_1] = {NoFlags.NO_FLAGS}
possible_flags1[pack4_v1_1] = {NoFlags.NO_FLAGS, "D"}
possible_flags1[pack4_v2_1] = {NoFlags.NO_FLAGS, "D"}
possible_flags1[pack5_v1_1] = {NoFlags.NO_FLAGS}


class TestSolverEngineAndBuildRulesConstructor:
    @pytest.mark.skip(reason="Broken solver")
    def test1_middle_size_with_flags(self):
        engine = SolverEngine(
            gathered_configs=gathered_configs1,
            versions_by_universal_name=versions_by_universal_name1,
            possible_flags=possible_flags1,
        )
        engine.run_engine([
            (SolverPackage(main_uni_name1, version=LocalEntry(path=Path.cwd())), [NoFlags.NO_FLAGS, "C"])
        ])
        constructor = BuildRulesConstructor(
            gathered_configs=gathered_configs1,
            variables_dep_version=engine.variables_dep_version,
            variables_dep_flag=engine.variables_dep_flag,
            variables_bare=engine.variables_bare,
            variables_flag=engine.variables_flag,
            model=engine.model,
        )
        constructor.construct_instructions()
        assert constructor.all_flags[main_v1_1] == ["C"]
        assert constructor.all_flags[pack5_v1_1] == []
        assert constructor.instructions[pack5_v1_1] == {}
        assert constructor.all_flags[pack2_v1_1] == ["B"]
        assert (pack1_v1_1 in constructor.all_flags) ^ (pack1_v2_1 in constructor.all_flags)
        if pack1_v1_1 in constructor.all_flags:
            assert constructor.instructions[main_v1_1] == {
                pack1_v1_1: ["A"],
                pack2_v1_1: ["B"],
                pack5_v1_1: [],
            }
            assert constructor.instructions[pack1_v1_1] == {pack3_v1_1: []}
            assert constructor.instructions[pack3_v1_1] == {}
            assert pack3_v2_1 not in constructor.instructions
            assert pack1_v2_1 not in constructor.instructions
            assert constructor.all_flags[pack1_v1_1] == ["A"]
            assert constructor.all_flags[pack3_v1_1] == []
            assert pack3_v2_1 not in constructor.all_flags
        else:
            assert constructor.instructions[main_v1_1] == {
                pack1_v2_1: ["A"],
                pack2_v1_1: ["B"],
                pack5_v1_1: [],
            }
            assert constructor.instructions[pack1_v2_1] == {pack3_v2_1: []}
            assert constructor.instructions[pack3_v2_1] == {}
            assert pack3_v1_1 not in constructor.instructions
            assert pack1_v1_1 not in constructor.instructions
            assert constructor.all_flags[pack1_v2_1] == ["A"]
            assert constructor.all_flags[pack3_v2_1] == []
            assert pack3_v1_1 not in constructor.all_flags
        assert (pack4_v1_1 in constructor.all_flags) ^ (pack4_v2_1 in constructor.all_flags)
        if pack4_v1_1 in constructor.all_flags:
            assert constructor.instructions[pack2_v1_1] == {pack4_v1_1: ["D"]}
            assert constructor.instructions[pack4_v1_1] == {}
            assert pack4_v2_1 not in constructor.instructions
            assert constructor.all_flags[pack4_v1_1] == ["D"]
        else:
            assert constructor.instructions[pack2_v1_1] == {pack4_v2_1: ["D"]}
            assert constructor.instructions[pack4_v2_1] == {}
            assert pack4_v1_1 not in constructor.instructions
            assert constructor.all_flags[pack4_v2_1] == ["D"]
