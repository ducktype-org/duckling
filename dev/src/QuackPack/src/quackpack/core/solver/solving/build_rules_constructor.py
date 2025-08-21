from dataclasses import dataclass

from quackpack.core.solver.solving.package_and_dependency import PackageAndDependencyId
from quackpack.core.solver.solving.solver_model import SolverModelOutput
from quackpack.core.solver.types.unresolved_id import IdResolvents
from quackpack.core.solver.types.unresolved_package import ResolvedPackage
from quackpack.core.types.manifest.summary import Summary
from quackpack.util.logger import get_logger
from quackpack.util.types.pkgid import Identifier

logger = get_logger(__name__)


@dataclass
class BuildRules:
    flags_to_install: dict[ResolvedPackage, list[Identifier]]
    instructions: dict[ResolvedPackage, dict[Identifier, ResolvedPackage]]


class _BuildRulesConstructor:
    def __init__(
        self,
        summaries: dict[ResolvedPackage, Summary],
        solution: SolverModelOutput,
        id_resolvents: IdResolvents,
    ):
        self.summaries = summaries
        self.solution = solution
        self.id_resolvents = id_resolvents

    def _construct_for_single_package(self, output: BuildRules, package: ResolvedPackage) -> None:
        lit_flags = self.solution.package_flags[package]
        output.flags_to_install[package] = list(lit_flags)
        output.instructions[package] = {}

        for dependency_alias, dependency_description in self.summaries[package].deps.items():
            dependency = PackageAndDependencyId.from_dependency(
                package, dependency_description, self.id_resolvents
            )
            realization = next(iter(self.solution.dep_versions[dependency]))
            output.instructions[package][dependency_alias] = realization

    def construct_all_instructions(self, output: BuildRules) -> None:
        for package in self.solution.packages:
            self._construct_for_single_package(output, package)


def construct_build_rules(
    solution: SolverModelOutput, summaries: dict[ResolvedPackage, Summary], id_resolvents: IdResolvents
) -> BuildRules:
    rules = BuildRules(flags_to_install={}, instructions={})
    _BuildRulesConstructor(summaries, solution, id_resolvents).construct_all_instructions(rules)
    return rules
