use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};

use tempfile::TempDir;
use tracing::{debug, error};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::fetcher::types::{
    FetcherResponse, GitCloneResponse, MultiMetadata, PackageWithUrl,
};
use crate::quackpack::core::solver::gathering::error_surpression::{
    GathererComputation, GathererResult,
};
use crate::quackpack::core::solver::gathering::fetch_types::{
    FetchFailure, FetchResponse, FetchSuccess, ManifestsRequest, NotPinnedFailure,
    NotPinnedRequest, NotPinnedSuccess, PinnedFailure, PinnedRequest, PinnedSuccess,
};
use crate::quackpack::core::solver::gathering::gatherer_state::{
    GatheredInfo, GathererState, RequestAction,
};
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::solver::types_common::{
    ExpandedLocation, ExpandedPackage, InternedLocation, Location,
};
use crate::quackpack::core::{FeatureName, GitReference, Manifest, PackageLoader};
use crate::quackpack::schemas::registry;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_path_buf::ToPathBuf;
use crate::quackpack::util::to_url::ToUrl;
use crate::{QuackResult, QuackResultContext, StrId, qp_bail_internal};

/// A struct for fetching manifests for all the packages potentially used in the dependency resolution.
pub struct Gatherer<'duck, 'fetcher, 'access, Access: GitAccess> {
    fetcher: &'fetcher mut Fetcher<'duck>,
    git_access: &'access mut Access,
}

impl<'duck, 'fetcher, 'access, Access: GitAccess> Gatherer<'duck, 'fetcher, 'access, Access> {
    /// Creates a new, empty [`Gatherer`].
    pub fn new(fetcher: &'fetcher mut Fetcher<'duck>, git_access: &'access mut Access) -> Self {
        Self {
            fetcher,
            git_access,
        }
    }

    /// Main entry point, explores the dependency graph of the root package in a BFS-like manner.
    /// For a given dependency entry in a manifest, fetches the manifests of the potential realizations
    /// and repeats the process for their manifests.
    #[tracing::instrument(skip_all, fields(mode, offline = self.fetcher.ctx().is_offline()))]
    pub fn explore(
        &mut self,
        root_path: PathBuf,
        root_manifest: Manifest,
        root_features: HashSet<FeatureName>,
        mode: SolverMode,
    ) -> QuackResult<GatheredInfo> {
        let mut state = GathererState::default();
        let root_fetch_result =
            self.fetch_root(root_path, root_manifest, root_features, &mut state)?;

        let mut errors = vec![];
        let mut fetches = vec![];
        let mut requests = state
            .handle_fetch_response(root_fetch_result)?
            .dump_errors(&mut errors);
        while !fetches.is_empty() || !requests.is_empty() {
            if let Some(request) = requests.pop() {
                let action = state
                    .get_request_action(request.clone())?
                    .dump_errors(&mut errors);
                match action {
                    RequestAction::Fetch => fetches.push(request),
                    RequestAction::More {
                        requests: new_requests,
                    } => requests.extend(new_requests),
                }
            }
            if !fetches.is_empty() {
                let request = fetches.remove(0);
                let fetch_result = self.fetch(request)?;
                let response = fetch_result.dump_errors(&mut errors);
                requests.extend(
                    state
                        .handle_fetch_response(response)?
                        .dump_errors(&mut errors),
                );
            }
        }
        if !errors.is_empty() {
            if mode.supress_foreign_manifests_errors {
                for e in errors {
                    self.fetcher.ctx().error_console().info_verbose(format!(
                        "Error\n{e}\nsuppressed due to the Merciful mode of the solver",
                    ))?;
                }
            } else {
                return Err(errors.into_iter().next().unwrap());
            }
        }
        state.try_into()
    }

    /// Helper for [`Gatherer::explore()`], creates a dummy [`ManifestsRequest`] for the root package to update the state
    /// and returns a dummy [`FetchResponse`], to create a starting point for the [`Gatherer::explore()`] function.
    ///
    /// Note: We assume that `root_features` are expanded.
    #[tracing::instrument(skip_all, fields(root_path))]
    fn fetch_root(
        &self,
        root_path: PathBuf,
        root_manifest: Manifest,
        root_features: HashSet<FeatureName>,
        state: &mut GathererState,
    ) -> QuackResult<FetchResponse> {
        if cfg!(debug_assertions) {
            assert_root_features_are_expanded(&root_manifest, &root_features);
        }
        let url = InternedUrl::new(root_path.to_url()?);
        let root_loc = InternedLocation::new(Location::Local { path: url });
        let root_request = NotPinnedRequest {
            location: root_loc,
            versions: None,
            features: root_features,
        };
        // The action is always RequestActionFetch, so ignore retured value.
        let action = state.get_request_action(ManifestsRequest::NotPinned(root_request))?;
        if let Some(e) = action.1.into_iter().next() {
            return Err(e)
                .context_internal("Request action for the first request should never have errors");
        }
        Ok(FetchResponse::Success(FetchSuccess::NotPinned(
            NotPinnedSuccess {
                origin_location: root_loc,
                fetched_manifests: HashMap::from([(
                    ExpandedPackage {
                        location: ExpandedLocation::Local { absolute_path: url },
                        version: None,
                    },
                    Box::new(root_manifest),
                )]),
            },
        )))
    }

    /// Helper for [`Gatherer::explore()`], performs a fetch.
    #[tracing::instrument(skip_all)]
    pub fn fetch(&mut self, request: ManifestsRequest) -> GathererResult<FetchResponse> {
        debug!(?request);
        match request {
            ManifestsRequest::Pinned(pinned_request) => self.fetch_registry_pinned(pinned_request),
            ManifestsRequest::NotPinned(not_pinned_request) => {
                match not_pinned_request.location.as_ref() {
                    Location::Registry { url, real_name } => {
                        Ok(self.fetch_registry_not_pinned(&not_pinned_request, *url, *real_name))
                    }
                    Location::Git { url, reference } => {
                        Ok(self.fetch_git(&not_pinned_request, *url, *reference))
                    }
                    Location::Local { path: path_url } => {
                        let path = path_url.to_path_buf()?;
                        Ok(self.fetch_local(&not_pinned_request, &path, *path_url))
                    }
                }
            }
        }
    }

    /// Helper for [`Gatherer::explore()`], performs a pinned registry fetch
    /// (registry fetch with a specified version).
    fn fetch_registry_pinned(&mut self, request: PinnedRequest) -> GathererResult<FetchResponse> {
        debug!("fetching registry (pinned)");
        let fetch_failure = || {
            FetchResponse::Failed(FetchFailure::Pinned(PinnedFailure {
                origin_location: request.location,
                origin_version: request.version,
            }))
        };
        let Location::Registry { url, real_name } = request.location.as_ref() else {
            qp_bail_internal!("Tried to make pinned registry fetch for a non-registry location");
        };
        let pkg_to_fetch = PackageWithUrl {
            id: *real_name,
            version: request.version,
            url: *url,
        };
        let fetcher_response: GathererComputation<Option<FetcherResponse<registry::Manifest>>> =
            self.fetcher.get_package_metadata(&pkg_to_fetch).into();
        let Some(fetcher_response) = fetcher_response.0 else {
            return Ok(GathererComputation(fetch_failure(), fetcher_response.1));
        };
        let expanded_loc = ExpandedLocation::Registry {
            url: *url,
            real_name: *real_name,
        };
        let FetcherResponse::Some(registry_manifest) = fetcher_response else {
            return Ok(GathererComputation::only_success(fetch_failure()));
        };
        let manifest: QuackResult<Manifest> = registry_manifest.try_into();
        let computation = match manifest {
            Ok(manifest) => GathererComputation::only_success(FetchResponse::Success(
                FetchSuccess::Pinned(PinnedSuccess {
                    origin_location: request.location,
                    origin_version: request.version,
                    expanded_package: ExpandedPackage {
                        location: expanded_loc,
                        version: Some(manifest.version()),
                    },
                    fetched_manifest: Box::new(manifest),
                }),
            )),
            Err(e) => {
                debug!("failed to fetch: {e}");
                GathererComputation(fetch_failure(), vec![e])
            }
        };
        Ok(computation)
    }

    /// Helper for [`Gatherer::explore()`], performs a not pinned registry fetch
    /// (registry fetch of all the versions of some package).
    fn fetch_registry_not_pinned(
        &mut self,
        request: &NotPinnedRequest,
        url: InternedUrl,
        real_name: StrId,
    ) -> GathererComputation<FetchResponse> {
        debug!("fetching registry (not pinned)");
        let fetch_failure = || {
            FetchResponse::Failed(FetchFailure::NotPinned(NotPinnedFailure {
                origin_location: InternedLocation::new(Location::Registry { url, real_name }),
            }))
        };
        let fetcher_response: GathererComputation<Option<FetcherResponse<MultiMetadata>>> =
            self.fetcher.get_package_all_metadata(url, real_name).into();
        let Some(fetcher_response) = fetcher_response.0 else {
            return GathererComputation(fetch_failure(), fetcher_response.1);
        };
        let FetcherResponse::Some(fetcher_response) = fetcher_response else {
            return GathererComputation::only_success(fetch_failure());
        };
        let expanded_loc = ExpandedLocation::Registry { url, real_name };
        let mut fetch_response = NotPinnedSuccess {
            origin_location: request.location,
            fetched_manifests: HashMap::new(),
        };
        let mut errors = vec![];
        for manifest in fetcher_response.packages_metadata {
            let manifest: QuackResult<Manifest> = manifest.try_into();
            match manifest {
                Ok(manifest) => {
                    fetch_response.fetched_manifests.insert(
                        ExpandedPackage {
                            location: expanded_loc,
                            version: Some(manifest.version()),
                        },
                        Box::new(manifest),
                    );
                }
                Err(e) => {
                    debug!("failed to fetch: {e}");
                    errors.push(e);
                }
            }
        }
        GathererComputation(
            FetchResponse::Success(FetchSuccess::NotPinned(fetch_response)),
            errors,
        )
    }

    /// Helper for [`Gatherer::explore()`], performs a git fetch
    /// (fetch from an external git repository).
    fn fetch_git(
        &mut self,
        request: &NotPinnedRequest,
        url: InternedUrl,
        reference: GitReference,
    ) -> GathererComputation<FetchResponse> {
        debug!("fetching git");
        let fetch_failure = || {
            FetchResponse::Failed(FetchFailure::NotPinned(NotPinnedFailure {
                origin_location: InternedLocation::new(Location::Git { url, reference }),
            }))
        };
        match self.try_get_cached_git(url, reference) {
            Ok(Some(success)) => {
                debug!("git request was cached");
                return GathererComputation::only_success(FetchResponse::Success(
                    FetchSuccess::NotPinned(success),
                ));
            }
            Ok(None) => {}
            Err(e) => {
                error!("failed to get cached git: {e}");
                // return GathererComputation::only_error(e);
            }
        }
        if self.fetcher.ctx().is_offline() {
            return GathererComputation::only_success(fetch_failure());
        }

        let fetcher_response: GathererComputation<Option<(GitCloneResponse, TempDir)>> =
            self.fetcher.clone_from_git(&url, reference).into();
        let Some((cloned_pkg, path_where_cloned)) = fetcher_response.0 else {
            return GathererComputation(fetch_failure(), fetcher_response.1);
        };
        let expanded_loc = ExpandedLocation::Git {
            url,
            commit: cloned_pkg.commit_hash,
        };
        if !self.git_access.is_stored(url, &cloned_pkg.commit_hash)
            && let Err(e) =
                self.git_access
                    .store(url, &cloned_pkg.commit_hash, path_where_cloned.path())
        {
            debug!("failed to store a new git: {e}");
            return GathererComputation(fetch_failure(), vec![e]);
        }
        let expanded_pkg = ExpandedPackage {
            location: expanded_loc,
            version: None,
        };
        GathererComputation::only_success(FetchResponse::Success(FetchSuccess::NotPinned(
            NotPinnedSuccess {
                origin_location: request.location,
                fetched_manifests: HashMap::from([(
                    expanded_pkg,
                    Box::new(cloned_pkg.package.manifest().clone()),
                )]),
            },
        )))
    }

    fn try_get_cached_git(
        &self,
        url: InternedUrl,
        reference: GitReference,
    ) -> QuackResult<Option<NotPinnedSuccess>> {
        if let GitReference::Rev(commit) = reference
            && let Some(path) = self.git_access.path_if_stored(url, &commit)
        {
            let origin_location = InternedLocation::new(Location::Git { url, reference });
            let expanded_location = ExpandedLocation::Git { url, commit };
            let storage_local_request = NotPinnedRequest {
                location: InternedLocation::new(Location::Local {
                    path: path.to_url()?.into(),
                }),
                versions: None,
                features: [].into(),
            };
            let path_url = path.to_url()?.into();
            let stored_git_manifest = self.fetch_local(&storage_local_request, &path, path_url);
            if let FetchResponse::Success(FetchSuccess::NotPinned(success)) = stored_git_manifest.0
                && let Some(manifest) = success.fetched_manifests.into_values().next()
            {
                let pkg = ExpandedPackage {
                    location: expanded_location,
                    version: None,
                };
                return Ok(Some(NotPinnedSuccess {
                    origin_location,
                    fetched_manifests: [(pkg, manifest)].into(),
                }));
            }
        }
        Ok(None)
    }

    /// Helper for [`Gatherer::explore()`], performs a local fetch
    /// (fetch from a given path).
    fn fetch_local(
        &self,
        request: &NotPinnedRequest,
        path: &Path,
        url: InternedUrl,
    ) -> GathererComputation<FetchResponse> {
        debug!("fetching local");
        let pcx = PackageLoader::find_at_exact_directory(path, self.fetcher.ctx());
        match pcx {
            Ok(pcx) => {
                let exp_pkg = ExpandedPackage {
                    location: ExpandedLocation::Local { absolute_path: url },
                    version: None,
                };
                GathererComputation::only_success(FetchResponse::Success(FetchSuccess::NotPinned(
                    NotPinnedSuccess {
                        origin_location: request.location,
                        fetched_manifests: HashMap::from([(
                            exp_pkg,
                            Box::new(pcx.package().manifest().clone()),
                        )]),
                    },
                )))
            }
            Err(e) => {
                debug!("failed to parse a package: {e}");
                GathererComputation(
                    FetchResponse::Failed(FetchFailure::NotPinned(NotPinnedFailure {
                        origin_location: InternedLocation::new(Location::Local { path: url }),
                    })),
                    vec![e],
                )
            }
        }
    }
}

fn assert_root_features_are_expanded(
    root_manifest: &Manifest,
    root_features: &HashSet<FeatureName>,
) {
    assert_eq!(
        root_manifest
            .features()
            .expand_features(root_features.iter().copied())
            .unwrap(),
        *root_features
    );
}
