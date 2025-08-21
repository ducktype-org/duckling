from pathlib import Path

from quackpack.core.fetcher.fetcher import FetcherContext
from quackpack.core.solver.cleaning import clean_gathered_info
from quackpack.core.solver.gathering import GatheredInfo, explore
from quackpack.core.solver.solving import construct_build_rules, run_engine
from quackpack.core.solver.types.git_access import GitAccess
from quackpack.core.solver.types.resolved_id import ResolvedIdLocal
from quackpack.core.solver.types.solver_mode import SolverMode
from quackpack.core.solver.types.unresolved_package import ResolvedPackageLocal
from quackpack.core.solver.util import create_git_fetch_cache
from quackpack.core.storage.files import PackageFreeze, VenvFreeze
from quackpack.core.types.manifest.summary import Summary
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier

logger = get_logger(__name__)


class Solver:
    def __init__(
        self,
        ctx: GlobalContext,
        root_path: Path,
        root_summary: Summary,
        root_flags: set[Identifier],
        mode: SolverMode = SolverMode.STRICT,
    ):
        self.ctx = ctx
        self.mode = mode
        self.root_path = root_path.expanduser().resolve()
        self.root_summary = root_summary
        self.root_flags = root_flags

    async def prepare_solving(self, git_access: GitAccess) -> GatheredInfo:
        with FetcherContext(self.ctx) as fetcher:
            return await explore(
                self.ctx,
                fetcher,
                git_access,
                root_path=self.root_path,
                root_summary=self.root_summary,
                root_flags=self.root_flags,
                mode=self.mode,
            )

    def finish_solving(self, gathered_info: GatheredInfo) -> VenvFreeze:
        root_package = ResolvedPackageLocal(ResolvedIdLocal(self.root_path))

        # With STRICT mode, any missing data aborts the gatherer, so there is nothing to clean.
        if self.mode is not SolverMode.STRICT:
            gathered_info = clean_gathered_info(input=gathered_info)
        if root_package not in gathered_info.possible_features:
            raise QuackPackError("Cannot satisfy dependencies!")

        solution = run_engine(data=gathered_info, root_projects=[(root_package, self.root_flags)])
        if root_package not in solution.packages:
            # this should be unreachable if info cleaner and its
            # error handling works correctly, but it does not hurt
            raise QuackPackError("Cannot satisfy dependencies!")

        build_rules = construct_build_rules(
            solution=solution, summaries=gathered_info.summaries, id_resolvents=gathered_info.id_resolvents
        )

        return VenvFreeze(
            direct_dependencies={
                alias: dep.to_package_id() for alias, dep in build_rules.instructions[root_package].items()
            },
            dependencies={
                id.to_package_id(): PackageFreeze(
                    dependencies={
                        alias: dep.to_package_id() for alias, dep in build_rules.instructions[id].items()
                    },
                    used_flags=flags,
                )
                for id, flags in build_rules.flags_to_install.items()
                # exclude the root package from freeze file, as to not immediately prevent
                # venv portability by storing the project path in the freeze file
                if id != root_package
            },
            git_fetch_cache=create_git_fetch_cache(
                data=gathered_info, used_packages=build_rules.flags_to_install.keys()
            ),
        )
