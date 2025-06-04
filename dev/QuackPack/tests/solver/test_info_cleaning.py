from pathlib import Path

from pydantic import AnyUrl

from quackpack.config.project import GitEntry, LocalEntry
from quackpack.solver.info_cleaning import InfoCleaner
from quackpack.solver.util import NoFlags, SolverPackage, SolverPackageWithFlag, UniversalName
from quackpack.util.pkgid import Identifier
from quackpack.util.version import Version

# Assets for test 1
main_uni_name1 = UniversalName.local_uni_name(Path.cwd())
pack1_uni_name1 = UniversalName.server_uni_name(Identifier("pack1"), AnyUrl("http://www.google.com"))
pack2_uni_name1 = UniversalName.server_uni_name(Identifier("pack2"), AnyUrl("http://www.google.com"))
pack3_uni_name1 = UniversalName.server_uni_name(Identifier("pack3"), AnyUrl("http://www.google.com"))
pack4_uni_name1 = UniversalName.server_uni_name(Identifier("pack4"), AnyUrl("http://www.google.com"))

main_v1_1 = SolverPackage(name=main_uni_name1, version=LocalEntry(path=Path.cwd()))
pack1_v1_1 = SolverPackage(name=pack1_uni_name1, version=Version(1))
pack1_v2_1 = SolverPackage(name=pack1_uni_name1, version=Version(2))
pack2_v1_1 = SolverPackage(name=pack2_uni_name1, version=Version(1))
pack3_v1_1 = SolverPackage(name=pack3_uni_name1, version=Version(1))
pack4_v1_1 = SolverPackage(name=pack4_uni_name1, version=Version(1))

main_v1_N_1 = SolverPackageWithFlag(package=main_v1_1, flag=NoFlags.NO_FLAGS)
pack1_v1_N_1 = SolverPackageWithFlag(package=pack1_v1_1, flag=NoFlags.NO_FLAGS)
pack1_v2_N_1 = SolverPackageWithFlag(package=pack1_v2_1, flag=NoFlags.NO_FLAGS)
pack2_v1_N_1 = SolverPackageWithFlag(package=pack2_v1_1, flag=NoFlags.NO_FLAGS)
pack3_v1_N_1 = SolverPackageWithFlag(package=pack3_v1_1, flag=NoFlags.NO_FLAGS)
pack4_v1_N_1 = SolverPackageWithFlag(package=pack4_v1_1, flag=NoFlags.NO_FLAGS)

possible_flags1: dict[SolverPackage, set[NoFlags | str]] = {}
possible_flags1[main_v1_1] = {NoFlags.NO_FLAGS}
possible_flags1[pack1_v1_1] = {NoFlags.NO_FLAGS}
possible_flags1[pack1_v2_1] = {NoFlags.NO_FLAGS}
possible_flags1[pack2_v1_1] = {NoFlags.NO_FLAGS}
possible_flags1[pack3_v1_1] = {NoFlags.NO_FLAGS}
possible_flags1[pack4_v1_1] = {NoFlags.NO_FLAGS}

fails1: set[UniversalName] = {pack4_uni_name1}

parents1: dict[SolverPackageWithFlag, list[SolverPackageWithFlag]] = {}
parents1[pack1_v1_N_1] = [main_v1_N_1, pack3_v1_N_1]
parents1[pack1_v2_N_1] = [main_v1_N_1]
parents1[pack2_v1_N_1] = [pack1_v1_N_1]
parents1[pack3_v1_N_1] = [pack2_v1_N_1]
parents1[pack4_v1_N_1] = [pack3_v1_N_1]

n_children1: dict[SolverPackageWithFlag, dict[UniversalName, dict[str | NoFlags, int]]] = {}
n_children1[main_v1_N_1] = {pack1_uni_name1: {NoFlags.NO_FLAGS: 2}}
n_children1[pack1_v1_N_1] = {pack2_uni_name1: {NoFlags.NO_FLAGS: 1}}
n_children1[pack2_v1_N_1] = {pack3_uni_name1: {NoFlags.NO_FLAGS: 1}}
n_children1[pack3_v1_N_1] = {pack1_uni_name1: {NoFlags.NO_FLAGS: 1}, pack4_uni_name1: {NoFlags.NO_FLAGS: 1}}

versions_by_universal_name1: dict[UniversalName, list[Version | GitEntry | LocalEntry]] = {}
versions_by_universal_name1[main_uni_name1] = [LocalEntry(path=Path.cwd())]
versions_by_universal_name1[pack1_uni_name1] = [Version(1), Version(2)]
versions_by_universal_name1[pack2_uni_name1] = [Version(1)]
versions_by_universal_name1[pack3_uni_name1] = [Version(1)]
versions_by_universal_name1[pack4_uni_name1] = [Version(1)]


class TestInfoCleaning:
    def test1_info_cleaning(self):
        cleaner = InfoCleaner(
            possible_flags=possible_flags1,
            fails=fails1,
            flag_fails=set(),
            parents=parents1,
            n_children=n_children1,
            versions_by_universal_name=versions_by_universal_name1,
        )
        cleaner.clean()
        assert versions_by_universal_name1 == {
            pack1_uni_name1: [Version(2)],
            main_uni_name1: [LocalEntry(path=Path.cwd())],
        }
        assert possible_flags1 == {pack1_v2_1: {NoFlags.NO_FLAGS}, main_v1_1: {NoFlags.NO_FLAGS}}
