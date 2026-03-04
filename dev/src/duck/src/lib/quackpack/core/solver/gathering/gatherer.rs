use async_scoped::TokioScope;
use futures::future::select_all;
use std::{
    collections::{HashMap, HashSet},
    path::{Path, PathBuf},
    sync::Arc,
};
use tempfile::TempDir;
use url::Url;

use tokio::sync::Mutex;

use crate::{
    QpCtx, QuackResult, QuackResultContext, StrId,
    duck::util::indent::indent,
    qp_bail_internal,
    quackpack::{
        core::{
            BranchOrTag, FeatureName, Git, Manifest, PackageLoader,
            fetcher::{
                Fetcher,
                types::{FetcherResponse, GitCloneResponse, MultiMetadata, PackageWithUrl},
            },
            gathering::{
                error_surpression::{GathererComputation, GathererResult},
                fetch_types::{
                    FetchFailure, FetchResponse, FetchSuccess, ManifestsRequest, NotPinnedFailure,
                    NotPinnedRequest, NotPinnedSuccess, PinnedFailure, PinnedRequest,
                    PinnedSuccess,
                },
                gatherer_state::{GatheredInfo, GathererState, RequestAction},
            },
            git_access::GitAccess,
            solver_mode::SolverMode,
            types_common::{
                ExpandedLocation, ExpandedPackage, InternedExpandedLocation, InternedLocation,
                Location,
            },
        },
        schemas::registry,
        util::async_helpers::{extract_single_item_from_vec, unpack_tokio_scoped_vector},
    },
};

/// A struct for fetching manifests for all the packages potentially used in the dependency resolution.
pub struct Gatherer<'duck, GitAccessImpl: GitAccess> {
    ctx: &'duck QpCtx<'duck>,
    fetcher: &'duck Fetcher<'duck>,
    git_access: Arc<Mutex<GitAccessImpl>>,
}

impl<'duck, GitAccessImpl: GitAccess> Gatherer<'duck, GitAccessImpl> {
    /// Creates a new, empty [`Gatherer`].
    pub fn new(
        ctx: &'duck QpCtx<'duck>,
        fetcher: &'duck Fetcher<'duck>,
        git_access: Arc<Mutex<GitAccessImpl>>,
    ) -> Self {
        Self {
            ctx,
            fetcher,
            git_access,
        }
    }

    /// Main entry point, explores the dependency graph of the root package in a BFS-like manner.
    /// For a given dependency entry in a manifest, fetches the manifests of the potential realizations
    /// and repeats the proccess for their manifests.
    pub async fn explore(
        &self,
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
                    RequestAction::Fetch => {
                        fetches.push(Box::pin(self.fetch(request, mode.offline)))
                    }
                    RequestAction::More {
                        requests: new_requests,
                    } => requests.extend(new_requests),
                }
            }
            if !fetches.is_empty() {
                let (fetch_result, _, remaining) = select_all(fetches).await;
                let response = fetch_result?.dump_errors(&mut errors);
                requests.extend(
                    state
                        .handle_fetch_response(response)?
                        .dump_errors(&mut errors),
                );
                fetches = remaining;
            }
        }
        if !errors.is_empty() {
            if mode.supress_foreign_manifests_errors {
                for e in errors {
                    self.ctx.error_console().info(format!(
                        "Error\n{}\nsuppressed due to the Merciful mode of the solver",
                        indent(&format!("{e}"), 6)
                    ));
                }
            } else {
                return Err(errors.into_iter().next().unwrap());
            }
        }
        state.into_gathered_info()
    }

    /// Helper for [`Gatherer::explore()`], creates a dummy [`ManifestsRequest`] for the root package to update the state
    /// and returns a dummy [`FetchResponse`], to create a starting point for the [`Gatherer::explore()`] function.
    fn fetch_root(
        &self,
        root_path: PathBuf,
        root_manifest: Manifest,
        root_features: HashSet<FeatureName>,
        state: &mut GathererState,
    ) -> QuackResult<FetchResponse> {
        let root_loc = InternedLocation::new(Location::Local {
            path: root_path.clone(),
        });
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
                        location: InternedExpandedLocation::new(ExpandedLocation::Local {
                            absolute_path: root_path,
                        }),
                        version: None,
                    },
                    Box::new(root_manifest),
                )]),
            },
        )))
    }

    /// Helper for [`Gatherer::explore()`], performs a fetch.
    pub async fn fetch(
        &self,
        request: ManifestsRequest,
        offline: bool,
    ) -> GathererResult<FetchResponse> {
        match request {
            ManifestsRequest::Pinned(pinned_request) => {
                self.fetch_registry_pinned(pinned_request, offline).await
            }
            ManifestsRequest::NotPinned(not_pinned_request) => {
                match not_pinned_request.location.as_ref() {
                    Location::Registry { url, real_name } => Ok(self
                        .fetch_registry_not_pinned(&not_pinned_request, url, *real_name, offline)
                        .await),
                    Location::Git {
                        url,
                        branch_or_tag,
                        rev,
                    } => {
                        self.fetch_git(&not_pinned_request, url, *branch_or_tag, *rev, offline)
                            .await
                    }
                    Location::Local { path } => self.fetch_local(&not_pinned_request, path),
                }
            }
        }
    }

    /// Helper for [`Gatherer::explore()`], performs a pinned registry fetch
    /// (registry fetch with a specified version).
    async fn fetch_registry_pinned(
        &self,
        request: PinnedRequest,
        offline: bool,
    ) -> GathererResult<FetchResponse> {
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
            url: url.clone(),
        };
        let fetcher_response: GathererComputation<Option<FetcherResponse<registry::Manifest>>> =
            self.fetcher
                .get_package_metadata(&pkg_to_fetch, offline)
                .await
                .into();
        let Some(fetcher_response) = fetcher_response.0 else {
            return Ok(GathererComputation(fetch_failure(), fetcher_response.1));
        };
        let expanded_loc = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: *real_name,
        });
        let FetcherResponse::Some(registry_manifest) = fetcher_response else {
            return Ok(GathererComputation::only_success(fetch_failure()));
        };
        let manifest: QuackResult<Manifest> = registry_manifest.try_into();
        Ok(match manifest {
            Ok(manifest) => GathererComputation::only_success(FetchResponse::Success(
                FetchSuccess::Pinned(PinnedSuccess {
                    origin_location: request.location,
                    origin_version: request.version,
                    expanded_package: ExpandedPackage {
                        location: expanded_loc,
                        version: Some(manifest.root_description().version()),
                    },
                    fetched_manifest: Box::new(manifest),
                }),
            )),
            Err(e) => GathererComputation(fetch_failure(), vec![e]),
        })
    }

    /// Helper for [`Gatherer::explore()`], performs a not pinned registry fetch
    /// (registry fetch of all the versions of some package).
    async fn fetch_registry_not_pinned(
        &self,
        request: &NotPinnedRequest,
        url: &Url,
        real_name: StrId,
        offline: bool,
    ) -> GathererComputation<FetchResponse> {
        let fetch_failure = || {
            FetchResponse::Failed(FetchFailure::NotPinned(NotPinnedFailure {
                origin_location: InternedLocation::new(Location::Registry {
                    url: url.clone(),
                    real_name,
                }),
            }))
        };
        let fetcher_response: GathererComputation<Option<FetcherResponse<MultiMetadata>>> = self
            .fetcher
            .get_package_all_metadata(url, real_name, offline)
            .await
            .into();
        let Some(fetcher_response) = fetcher_response.0 else {
            return GathererComputation(fetch_failure(), fetcher_response.1);
        };
        let FetcherResponse::Some(fetcher_response) = fetcher_response else {
            return GathererComputation::only_success(fetch_failure());
        };
        let expanded_loc = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name,
        });
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
                            version: Some(manifest.root_description().version()),
                        },
                        Box::new(manifest),
                    );
                }
                Err(e) => {
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
    async fn fetch_git(
        &self,
        request: &NotPinnedRequest,
        url: &Url,
        branch_or_tag: BranchOrTag,
        rev: Option<StrId>,
        offline: bool,
    ) -> GathererResult<FetchResponse> {
        let fetch_failure = || {
            FetchResponse::Failed(FetchFailure::NotPinned(NotPinnedFailure {
                origin_location: InternedLocation::new(Location::Git {
                    url: url.clone(),
                    branch_or_tag,
                    rev,
                }),
            }))
        };

        if let Some(success) = self.try_get_cached_git(url, branch_or_tag, rev).await? {
            return Ok(GathererComputation::only_success(FetchResponse::Success(
                FetchSuccess::NotPinned(success),
            )));
        }
        if offline {
            return Ok(GathererComputation::only_success(fetch_failure()));
        }

        let git_source = Git::new(url.clone(), branch_or_tag, rev);
        let fetcher_response: GathererComputation<Option<(GitCloneResponse, TempDir)>> =
            self.fetcher.clone_from_git(&git_source).await.into();
        let Some((cloned_pkg, path_where_cloned)) = fetcher_response.0 else {
            return Ok(GathererComputation(fetch_failure(), fetcher_response.1));
        };
        let expanded_loc = InternedExpandedLocation::new(ExpandedLocation::Git {
            url: url.clone(),
            commit: cloned_pkg.commit_hash,
        });
        let mut git_access = self.git_access.lock().await;
        if !git_access.is_stored(url.clone(), cloned_pkg.commit_hash)
            && let Err(e) = git_access.store(
                url.clone(),
                cloned_pkg.commit_hash,
                path_where_cloned.path(),
            )
        {
            return Ok(GathererComputation(fetch_failure(), vec![e]));
        }
        let expanded_pkg = ExpandedPackage {
            location: expanded_loc,
            version: None,
        };
        Ok(GathererComputation::only_success(FetchResponse::Success(
            FetchSuccess::NotPinned(NotPinnedSuccess {
                origin_location: request.location,
                fetched_manifests: HashMap::from([(
                    expanded_pkg,
                    Box::new(cloned_pkg.package.manifest().clone()),
                )]),
            }),
        )))
    }

    async fn try_get_cached_git(
        &self,
        url: &Url,
        branch_or_tag: BranchOrTag,
        rev: Option<StrId>,
    ) -> QuackResult<Option<NotPinnedSuccess>> {
        let git_access = self.git_access.lock().await;
        if matches!(branch_or_tag, BranchOrTag::Default)
            && let Some(commit) = rev
            && let Some(path) = git_access.path_if_stored(url.clone(), commit)
        {
            let origin_location = InternedLocation::new(Location::Git {
                url: url.clone(),
                branch_or_tag,
                rev,
            });
            let expanded_location = InternedExpandedLocation::new(ExpandedLocation::Git {
                url: url.clone(),
                commit,
            });
            let storage_local_request = NotPinnedRequest {
                location: InternedLocation::new(Location::Local { path: path.clone() }),
                versions: None,
                features: [].into(),
            };
            drop(git_access);
            let stored_git_manifest = self.fetch_local(&storage_local_request, &path)?;
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
    ) -> GathererResult<FetchResponse> {
        let res = TokioScope::scope_and_block(|spawner| {
            let fetch_local = || async {
                let pkg_ctx = PackageLoader::find_at_exact_directory(path, self.ctx);

                match pkg_ctx {
                    Ok(pkg_ctx) => {
                        let exp_pkg = ExpandedPackage {
                            location: InternedExpandedLocation::new(ExpandedLocation::Local {
                                absolute_path: path.to_path_buf(),
                            }),
                            version: None,
                        };
                        GathererComputation::only_success(FetchResponse::Success(
                            FetchSuccess::NotPinned(NotPinnedSuccess {
                                origin_location: request.location,
                                fetched_manifests: HashMap::from([(
                                    exp_pkg,
                                    Box::new(pkg_ctx.package().manifest().clone()),
                                )]),
                            }),
                        ))
                    }
                    Err(e) => GathererComputation(
                        FetchResponse::Failed(FetchFailure::NotPinned(NotPinnedFailure {
                            origin_location: InternedLocation::new(Location::Local {
                                path: path.to_path_buf(),
                            }),
                        })),
                        vec![e],
                    ),
                }
            };
            spawner.spawn(fetch_local());
        });
        extract_single_item_from_vec(unpack_tokio_scoped_vector(res.1)?)
    }
}
