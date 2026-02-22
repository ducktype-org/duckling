from dataclasses import dataclass
from logging import Logger
from pathlib import Path
from typing import cast

from quackpack.core.solver.gathering.fetch_types import (
    FetchRequest,
    FetchResult,
    PinnedFetchRequest,
    UnpinnedFetchRequest,
)
from quackpack.core.solver.gathering.gathered_info import GatheredInfo
from quackpack.core.solver.types.flag_type import FeatureId
from quackpack.core.solver.types.packages_by_id import PackagesById
from quackpack.core.solver.types.resolved_package import ResolvedPackage
from quackpack.core.solver.types.solver_mode import SolverMode
from quackpack.core.solver.types.unresolved_id import IdResolvents, UnresolvedId
from quackpack.core.solver.types.unresolved_package import UnresolvedPackage
from quackpack.core.types.manifest.summary import Summary
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.version import Version

logger = get_logger(__name__)


@dataclass
class PackageData:
    summary: Summary
    """
    Fetched summary for the package.
    """

    possible_features: set[FeatureId] | None
    """
    Features with which the package is considered.
    If `None`, no request referenced this package even though it was fetched.
    """

    local_root: Path | None
    """
    Root for resolving local dependencies of the package.
    When `None`, the package cannot have any local dependencies (even transitive ones).
    """

    def dep_requests(self) -> set[FetchRequest]:
        """
        Returns the set of the requests required to satisfy the dependencies of the package.
        """
        if self.possible_features is None:
            return set()
        result = set[FetchRequest]()
        for dep in self.summary.deps.values():
            if not dep.is_enabled(self.possible_features):
                continue

            id = UnresolvedId.from_dependency(dep)
            flags = frozenset(dep.enabled_features(self.possible_features))
            local_root = None
            if dep.source.is_local():
                if self.local_root is None:
                    raise QuackPackError("Cannot depend on local package from non-local package!")
                dep_path = dep.source.as_local().absolute_dir_root
                local_root = dep_path if dep_path.is_absolute() else self.local_root / dep_path

            result.add(
                PinnedFetchRequest(
                    local_root=local_root, flags=flags, pkg=UnresolvedPackage.create(id, dep.versions[0])
                )
                if dep.is_pinned
                else UnpinnedFetchRequest(
                    local_root=local_root, flags=flags, id=id, versions=tuple(dep.versions)
                )
            )

        return result


@dataclass
class Failed:
    pass


@dataclass
class Pending:
    requests: set[FetchRequest]
    local_root: Path | None


@dataclass
class Done:
    pass


type QueryState = Failed | Pending | Done


@dataclass
class RequestActionFetch:
    pass


@dataclass
class RequestActionMore:
    requests: set[FetchRequest]


type RequestAction = RequestActionFetch | RequestActionMore


class GathererState:
    def __init__(self, mode: SolverMode):
        self.mode = mode
        self._id_resolvents = IdResolvents()
        self._packages_by_id = PackagesById()
        self._unpinned: dict[UnresolvedId, QueryState] = {}
        self._pinned: dict[UnresolvedPackage, QueryState] = {}
        self._data: dict[ResolvedPackage, PackageData] = {}
        logger.debug("Created gatherer state")

    def get_request_action(self, request: FetchRequest) -> RequestAction:
        """
        Signals that we want to request a given package.
        We check the state, if it is already fetched and if so, we perform the request immediately
        (that is, we update possible flags bases on requested flags).
        Otherwise the returned action signals that the request needs to be sent to fetcher.
        The result of a fetch will need to be handled using `handle_response` method.
        """
        if isinstance(request, UnpinnedFetchRequest):
            return self._get_request_action_unpinned(request)
        elif isinstance(request, PinnedFetchRequest):
            return self._get_request_action_pinned(request)
        raise Exception("unreachable")

    def _get_request_action_unpinned(self, request: UnpinnedFetchRequest) -> RequestAction:
        logger.debug(f"Asked for request action for dependency {request.id!s}")
        state = self._unpinned.get(request.id)

        if state is None:
            logger.debug(f"Dependency {request.id!s} requires fetch")
            self._unpinned[request.id] = Pending(local_root=request.local_root, requests={request})
            return RequestActionFetch()

        elif isinstance(state, Pending):
            logger.debug(f"Dependency {request.id!s} is already requested; chaining request to fetch")
            assert state.local_root == request.local_root  # sanity check
            state.requests.add(request)
            return RequestActionMore(requests=set())

        elif isinstance(state, Done):
            logger.debug(f"Dependency {request.id!s} was already fetched; handling flags")
            return RequestActionMore(
                requests=self._update_flags_unpinned(
                    id=request.id, selector=list(request.versions), requested_flags=set(request.flags)
                )
            )

        # already failed before
        return RequestActionMore(requests=set())

    def _get_request_action_pinned(self, request: PinnedFetchRequest) -> RequestAction:
        logger.debug(f"Asked for request action for pinned dependency {request.pkg!s}")
        state = self._pinned.get(request.pkg)

        # If the pinned request has not been made, we may still have done an unpinned request for corresponding id.
        if state is None:
            unpinned_state = self._unpinned.get(request.pkg.id)

            if unpinned_state is None:
                logger.debug(f"Pinned dependency {request.pkg!s} requires fetch")
                self._pinned[request.pkg] = Pending(local_root=request.local_root, requests={request})
                return RequestActionFetch()

            elif isinstance(unpinned_state, Pending):
                logger.debug(
                    f"Pinned dependency {request.pkg!s} already requested through unpinned fetch; chaining request to fetch"
                )
                assert unpinned_state.local_root == request.local_root  # sanity check
                # We are adding a pinned request to the requests chained to the unpinned fetch.
                # This is not a bug - later the same method `_complete_requests` will be used
                # for handling chained requests for both types of fetches, so this pinnned
                # request will be properly handled when the unpinned fetch completes.
                unpinned_state.requests.add(request)
                return RequestActionMore(requests=set())

            elif isinstance(unpinned_state, Done):
                resolved_pkg = request.pkg.resolve(self._id_resolvents)
                if resolved_pkg not in self._data:
                    error_with_mercy(
                        logger,
                        self.mode,
                        f"Pinned dependency {request.pkg!s} was supposed to be already fetched by id fetch, but the pinned version does not exist!",
                    )
                    return RequestActionMore(requests=set())
                logger.debug(
                    f"Pinned dependency {request.pkg!s} was already fetched by id fetch; handling flags"
                )
                return RequestActionMore(requests=self._update_flags(resolved_pkg, set(request.flags)))

        elif isinstance(state, Pending):
            logger.debug(f"Pinned dependency {request.pkg!s} is already requested; chaining request to fetch")
            assert state.local_root == request.local_root  # sanity check
            state.requests.add(request)
            return RequestActionMore(requests=set())

        elif isinstance(state, Done):
            logger.debug(f"Pinned dependency {request.pkg!s} was already fetched; handling flags")
            return RequestActionMore(
                requests=self._update_flags(request.pkg.resolve(self._id_resolvents), set(request.flags))
            )

        # already failed before
        return RequestActionMore(requests=set())

    def handle_response(self, result: FetchResult) -> set[FetchRequest]:
        """
        Handles fetch result. Returns new requests required to be made.
        """
        if isinstance(result.request, UnresolvedId):
            return self._handle_response_unpinned(result)
        else:  # isinstance(result.request, UnresolvedPackage)
            return self._handle_response_pinned(result)

    def _handle_response_unpinned(self, result: FetchResult) -> set[FetchRequest]:
        id = cast(UnresolvedId, result.request)
        logger.debug(f"Handling fetch result of dependency {id!s}")
        state = self._unpinned.get(id)
        assert isinstance(state, Pending)

        if len(result.packages) == 0:
            self._fail_unpinned(id, f"Fetch of dependency {id!s} yielded invalid result")
            return set()

        self._id_resolvents.update(id, next(iter(result.packages)).id)
        self._insert_manifests(result, state.local_root)
        self._unpinned[id] = Done()

        return self._complete_requests(state.requests)

    def _handle_response_pinned(self, result: FetchResult) -> set[FetchRequest]:
        pkg = cast(UnresolvedPackage, result.request)
        logger.debug(f"Handling fetch result of pinned dependency {pkg!s}")
        state = self._pinned.get(pkg)
        assert isinstance(state, Pending)

        if len(result.packages) != 1:
            self._fail_pinned(pkg, f"Fetch of pinned dependency {pkg!s} yielded invalid result")
            return set()
        result_pkg = next(iter(result.packages))
        if result_pkg.version != pkg.version:
            self._fail_pinned(pkg, f"Fetch of pinned dependency {pkg!s} yielded wrong version")
            return set()

        self._id_resolvents.update(pkg.id, result_pkg.id)
        self._insert_manifests(result, state.local_root)
        self._pinned[pkg] = Done()

        return self._complete_requests(state.requests)

    def _complete_requests(self, requests: set[FetchRequest]) -> set[FetchRequest]:
        """
        Completes all given requests, adding their requested features to matching packages.
        Requires that all corresponding data has already been added to the state
        (that is _insert_manifests has been called for all request targets).
        """
        result = set[FetchRequest]()
        for request in requests:
            if isinstance(request, UnpinnedFetchRequest):
                result |= self._update_flags_unpinned(
                    id=request.id, selector=list(request.versions), requested_flags=set(request.flags)
                )
            elif isinstance(request, PinnedFetchRequest):
                packages = self._packages_by_id[request.pkg.id.resolve(self._id_resolvents)]
                matching = {pkg for pkg in packages if pkg.version == request.pkg.version}
                if len(matching) != 1:
                    self._fail_pinned(
                        request.pkg,
                        f"Request of pinned dependency {request.pkg!s} could not find matching version",
                    )
                    continue
                result |= self._update_flags(next(iter(matching)), set(request.flags))
            else:
                raise Exception("unreachable")
        return result

    def _update_flags_unpinned(
        self, id: UnresolvedId, selector: list[Version], requested_flags: set[FeatureId]
    ) -> set[FetchRequest]:
        """
        Requests new flags from all packages matching given id and version list,
        returns requests required to satisfy dependencies with the new flag set.
        If flag set for some package have not changed, no dependency requests are made.
        Requires that the data for the given id has already been added to the state
        (that is _insert_manifests has been called on a fetch result for the id).
        """
        requests = set[FetchRequest]()
        any_matched = False
        for pkg in self._packages_by_id[id.resolve(self._id_resolvents)]:
            # NOTE: Selector to any version should be None instead of [],
            # similarily to system and arch selectors.
            # However the amount of changes to schemas required for that
            # is not worth the effort.
            if (
                selector == []
                or pkg.version is None
                or any(ver.can_be_upgraded_to(pkg.version) for ver in selector)
            ):
                any_matched = True
                requests |= self._update_flags(pkg, requested_flags)
        if not any_matched:
            error_with_mercy(
                logger,
                self.mode,
                f"There is a dependency on package {id!s} with versions {selector!s}, but no matching versions exist!",
            )
        return requests

    def _update_flags(self, pkg: ResolvedPackage, requested_flags: set[FeatureId]) -> set[FetchRequest]:
        """
        Requests new flags from the package and returns requests required to satisfy
        its dependencies with the new flag set.
        If flag set for the package have not changed, no dependency requests are made.
        Requires that the data for the package has already been added to the state.
        (that is _insert_manifests has been called on a fetch result for its id or
        pinned one for the package itself)
        """
        pkg_data = self._data[pkg]
        nonexistent_flags = requested_flags.difference(pkg_data.summary.features)
        if nonexistent_flags:
            error_with_mercy(logger, self.mode, f"Flags {nonexistent_flags} do not exist for package")
            return set()
        if pkg_data.possible_features is None:
            pkg_data.possible_features = set(requested_flags)
            return pkg_data.dep_requests()
        if requested_flags.difference(pkg_data.possible_features):
            pkg_data.possible_features |= requested_flags
            return pkg_data.dep_requests()
        return set()

    def _insert_manifests(self, result: FetchResult, local_root: Path | None) -> None:
        """
        Updates following internal mappings based on the fetch result:
        - the resolved id -> resolved package,
        - resolved package -> manifest.
        """
        for pkg, manifest in result.packages.items():
            if pkg not in self._data:
                self._packages_by_id[pkg.id].add(pkg)
                self._data[pkg] = PackageData(possible_features=None, local_root=local_root, summary=manifest)

    def _fail_unpinned(self, id: UnresolvedId, reason: str) -> None:
        """
        Helper for marking a unpinned request as failed
        and raising error depending on solver mode.
        """
        self._unpinned[id] = Failed()
        error_with_mercy(logger, self.mode, reason)

    def _fail_pinned(self, id: UnresolvedPackage, reason: str) -> None:
        """
        Helper for marking a pinned request as failed
        and raising error depending on solver mode.
        """
        self._pinned[id] = Failed()
        error_with_mercy(logger, self.mode, reason)

    def into_gatherer_result(self) -> GatheredInfo:
        """
        Transforms the gatherer state into gatherer result.

        The object is consumed by this method.
        """
        summaries = dict[ResolvedPackage, Summary]()
        possible_features = dict[ResolvedPackage, set[FeatureId]]()
        to_remove = set[ResolvedPackage]()
        for pkg, data in self._data.items():
            if data.possible_features is not None:
                summaries[pkg] = data.summary
                possible_features[pkg] = data.possible_features
            else:
                to_remove.add(pkg)
        for pkgs in self._packages_by_id.values():
            # this is `pkgs.difference(to_remove)`, but that wrapper does not provide that
            for pkg in {pkg for pkg in pkgs if pkg in to_remove}:
                pkgs.remove(pkg)
        return GatheredInfo(
            summaries=summaries,
            possible_features=possible_features,
            packages_by_id=self._packages_by_id,
            id_resolvents=self._id_resolvents,
        )


def error_with_mercy(logger: Logger, mode: SolverMode, message: str) -> None:
    """
    Raises error or logs depending on solver mode.
    """
    if mode != SolverMode.STRICT:
        logger.debug(f"Error suppressed due to merciful mode: {message}")
    else:
        raise QuackPackError(message)
