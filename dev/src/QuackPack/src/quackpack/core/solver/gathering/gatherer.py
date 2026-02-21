import asyncio
from asyncio import Task
from pathlib import Path
from typing import cast

from quackpack.core.fetcher.api_types import Package
from quackpack.core.fetcher.fetcher import Fetcher
from quackpack.core.package_loader import PackageLoader
from quackpack.core.solver.gathering.fetch_types import (
    FetchFailure,
    FetchRequest,
    FetchResult,
    PinnedFetchRequest,
    UnpinnedFetchRequest,
)
from quackpack.core.solver.gathering.gathered_info import GatheredInfo
from quackpack.core.solver.gathering.gatherer_state import (
    GathererState,
    RequestActionMore,
    error_with_mercy,
)
from quackpack.core.solver.types.flag_type import FeatureId
from quackpack.core.solver.types.git_access import GitAccess
from quackpack.core.solver.types.resolved_id import (
    ResolvedIdGit,
    ResolvedIdLocal,
    ResolvedIdRegistry,
)
from quackpack.core.solver.types.resolved_package import (
    ResolvedPackage,
    ResolvedPackageGit,
    ResolvedPackageLocal,
    ResolvedPackageRegistry,
)
from quackpack.core.solver.types.solver_mode import SolverMode
from quackpack.core.solver.types.unresolved_id import (
    UnresolvedIdGit,
    UnresolvedIdLocal,
    UnresolvedIdRegistry,
)
from quackpack.core.solver.types.unresolved_package import UnresolvedPackageRegistry
from quackpack.core.types.manifest.parse import create_schema, summary_from_schema
from quackpack.core.types.manifest.source import GitSource, SourceKind
from quackpack.core.types.manifest.summary import Summary
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError

logger = get_logger(__name__)


async def explore(
    ctx: GlobalContext,
    fetcher: Fetcher,
    git_access: GitAccess,
    root_path: Path,
    root_summary: Summary,
    root_flags: set[FeatureId],
    mode: SolverMode = SolverMode.STRICT,
) -> GatheredInfo:
    """
    Gathers all the packages metadata required to solve dependencies of given root package.
    """
    tasks: list[Task[FetchResult | FetchFailure]] = []

    state = GathererState(mode)

    root_id = UnresolvedIdLocal(root_path)
    root_request = UnpinnedFetchRequest(
        id=root_id,
        versions=(root_summary.version,),
        flags=frozenset(root_flags),
        local_root=root_path,
    )
    # the action is always RequestActionFetch, so ignore return value
    state.get_request_action(root_request)
    root_fetch_result = FetchResult(
        request=root_id,
        packages={ResolvedPackageLocal(ResolvedIdLocal(root_path)): root_summary},
    )
    deps = state.handle_response(root_fetch_result)

    while deps or tasks:
        if deps:
            request = deps.pop()
            action = state.get_request_action(request)
            logger.debug(f"Handling request {request!s}, received action {action!s}")
            if isinstance(action, RequestActionMore):
                deps |= action.requests
            else:
                tasks.append(
                    asyncio.create_task(_fetch(fetcher, git_access, request, ctx))
                )
        else:
            done, pending = await asyncio.wait(
                tasks, return_when=asyncio.FIRST_COMPLETED
            )
            tasks = list(pending)
            for done_task in done:
                result = done_task.result()
                if isinstance(result, FetchFailure):
                    error_with_mercy(
                        logger,
                        mode,
                        f"Fetch of dependency {result.source_request.target_str()} failed: {result.reason}",
                    )
                    continue
                deps |= state.handle_response(result)

    return state.into_gatherer_result()


async def _fetch(
    fetcher: Fetcher, git_access: GitAccess, request: FetchRequest, ctx: GlobalContext
) -> FetchResult | FetchFailure:
    if isinstance(request, PinnedFetchRequest):
        # pinned request is currently only possible (and only makes sense) for registry dependencies
        assert request.pkg.id.kind() == SourceKind.Registry
        return await _fetch_registry_pinned(
            fetcher, request, pkg=cast(UnresolvedPackageRegistry, request.pkg)
        )
    elif isinstance(request, UnpinnedFetchRequest):
        match request.id.kind():
            case SourceKind.Registry:
                return await _fetch_registry_multi(
                    fetcher, request, id=cast(UnresolvedIdRegistry, request.id)
                )
            case SourceKind.Git:
                return await _fetch_git(
                    ctx,
                    fetcher,
                    git_access,
                    request,
                    id=cast(UnresolvedIdGit, request.id),
                )
            case SourceKind.Local:
                return _fetch_local(ctx, request, cast(UnresolvedIdLocal, request.id))
    raise Exception("unreachable")


async def _fetch_registry_pinned(
    fetcher: Fetcher, request: FetchRequest, pkg: UnresolvedPackageRegistry
) -> FetchResult | FetchFailure:
    result = await fetcher.get_package_metadata(
        instance_url=pkg.id.registry_url,
        package=Package(id=pkg.id.package_name, version=str(pkg.version)),
    )
    if result.result is None:
        return FetchFailure(request, reason="fetch did not succeed")
    try:
        summary = Summary.from_schema(result.result)
    except QuackPackError as err:
        return FetchFailure(request, reason=f"could not decode response: {err!s}")
    resolved_pkg = ResolvedPackageRegistry(
        _id=_resolve_registry_id(pkg.id), _version=pkg.version
    )
    return FetchResult(request=pkg, packages={resolved_pkg: summary})


async def _fetch_registry_multi(
    fetcher: Fetcher, request: FetchRequest, id: UnresolvedIdRegistry
) -> FetchResult | FetchFailure:
    result = await fetcher.get_package_all_metadata(
        instance_url=id.registry_url, package_name=id.package_name
    )
    if result.result is None:
        return FetchFailure(request, reason="fetch did not succeed")
    resolved_id = _resolve_registry_id(id)
    packages: dict[ResolvedPackage, Summary] = {}
    for spec in result.result.packages_metadata:
        try:
            summary = Summary.from_schema(spec)
        except QuackPackError as err:
            return FetchFailure(request, reason=f"could not decode response: {err!s}")
        resolved_pkg = ResolvedPackageRegistry(
            _id=resolved_id, _version=summary.spec.version
        )
        packages[resolved_pkg] = summary
    return FetchResult(request=id, packages=packages)


def _fetch_local(
    ctx: GlobalContext, request: FetchRequest, id: UnresolvedIdLocal
) -> FetchResult | FetchFailure:
    # The local_root tracks the current root of local packages
    # to deal with relative paths in local dependencies.
    assert request.local_root is not None
    manifest = PackageLoader.find_at_exact_directory(request.local_root, ctx).manifest
    resolved_pkg = ResolvedPackageLocal(
        _id=ResolvedIdLocal(local_path=request.local_root)
    )
    return FetchResult(request=id, packages={resolved_pkg: manifest.summary})


async def _fetch_git(
    ctx: GlobalContext,
    fetcher: Fetcher,
    git_access: GitAccess,
    request: FetchRequest,
    id: UnresolvedIdGit,
) -> FetchResult | FetchFailure:
    cached = git_access.get_cached_git(
        url=id.repository_url, commit=id.commit, branch=id.branch, tag=id.tag
    )

    if cached is None:
        result = await fetcher.clone_from_git(
            GitSource(
                git_url=id.repository_url,
                commit=id.commit,
                tag=id.tag,
                branch=id.branch,
            )
        )
        if result.result is None:
            return FetchFailure(request, reason="fetch did not succeed")
        try:
            summary = Summary.from_schema(result.result)
        except QuackPackError as err:
            return FetchFailure(request, reason=f"could not decode response: {err!s}")
        if not git_access.is_stored(url=id.repository_url, commit=result.commit_hash):
            git_access.store(
                url=id.repository_url,
                commit=result.commit_hash,
                source_path=result.destination_path,
            )
        else:
            logger.debug(f"Git depdendency {id!s} already present in storage")
        resolved_id = ResolvedIdGit(
            repository_url=id.repository_url, commit=result.commit_hash
        )

    else:
        storage_path = git_access.git_path(url=cached.url, commit=cached.commit)
        schema = create_schema(storage_path / PackageLoader.MANIFEST_NAME)
        # @TODO: #1598 I really don't like passing ctx here; maybe we need to additionally
        # store RegistryManifest in storage within packages? (currently ctx can
        # e.g. change the default registry, which doesn't sound right)
        summary = summary_from_schema(schema, storage_path, ctx)
        resolved_id = ResolvedIdGit(repository_url=cached.url, commit=cached.commit)

    resolved_pkg = ResolvedPackageGit(_id=resolved_id, _version=summary.spec.version)
    return FetchResult(request=id, packages={resolved_pkg: summary})


def _resolve_registry_id(id: UnresolvedIdRegistry) -> ResolvedIdRegistry:
    return ResolvedIdRegistry(
        package_name=id.package_name, registry_url=id.registry_url
    )
