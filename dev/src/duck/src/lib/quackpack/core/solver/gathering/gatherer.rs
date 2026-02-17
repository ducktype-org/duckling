use futures::future::select_all;
use std::{
    collections::{HashMap, HashSet},
    path::PathBuf,
    sync::Arc,
};

use tempfile::TempDir;
use tokio::sync::Mutex;

use crate::{
    QpCtx, QuackResult, QuackResultContext, qp_bail_internal,
    quackpack::{
        core::{
            FeatureName, Git, Manifest, PackageLoader, SolverMode,
            fetcher::{
                Fetcher,
                types::{GitCloneResponse, MultiMetadata, PackageWithUrl},
            },
            gathering::{
                error_surpression::{GathererComputation, GathererResult},
                fetch_types::{
                    FetchResult, ManifestsRequest, NotPinnedRequest, NotPinnedResult,
                    PinnedRequest, PinnedResult,
                },
                gatherer_state::{GathererState, RequestAction},
            },
            git_access::GitAccess,
            types_common::{
                ExpandedLocation, ExpandedPackage, InternedExpandedLocation, InternedLocation,
                Location,
            },
        },
        schemas::registry,
    },
};

pub struct Gatherer<'duck, GitAccessImpl: GitAccess> {
    ctx: &'duck QpCtx<'duck>,
    fetcher: &'duck Fetcher<'duck>,
    git_access: Arc<Mutex<GitAccessImpl>>,
}

impl<'duck, GitAccessImpl: GitAccess> Gatherer<'duck, GitAccessImpl> {
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

    pub async fn explore(
        &self,
        root_path: PathBuf,
        root_manifest: Manifest,
        root_features: HashSet<FeatureName>,
        mode: SolverMode,
    ) -> QuackResult<()> {
        let mut state = GathererState::new();
        let root_fetch_result =
            self.fetch_root(root_path, root_manifest, root_features, &mut state)?;

        let mut errors = vec![];
        let mut fetches = vec![];
        let mut requests = state
            .handle_response(root_fetch_result)?
            .dump_errors(&mut errors);
        while !fetches.is_empty() || !requests.is_empty() {
            if let Some(request) = requests.pop() {
                let action = state
                    .get_request_action(request.clone())?
                    .dump_errors(&mut errors);
                match action {
                    RequestAction::Fetch => fetches.push(Box::pin(self.fetch(request))),
                    RequestAction::More {
                        requests: new_requests,
                    } => requests.extend(new_requests),
                }
            }
            if !fetches.is_empty() {
                let (fetch_result, _, remaining) = select_all(fetches).await;
                if let Some(fetch_result) = fetch_result?.dump_errors(&mut errors) {
                    requests.extend(
                        state
                            .handle_response(fetch_result)?
                            .dump_errors(&mut errors),
                    );
                }
                fetches = remaining;
            }
        }
        if !errors.is_empty() {
            match mode {
                SolverMode::Merciful => {
                    for e in errors {
                        self.ctx.error_console().info(format!(
                            "Error {e} surpressed due to the Merciful mode of the solver"
                        ));
                    }
                }
                SolverMode::Strict => {
                    return Err(errors.into_iter().next().unwrap());
                }
            }
        }
        Ok(())
    }

    fn fetch_root(
        &self,
        root_path: PathBuf,
        root_manifest: Manifest,
        root_features: HashSet<FeatureName>,
        state: &mut GathererState,
    ) -> QuackResult<FetchResult> {
        let root_loc = InternedLocation::new(Location::Local {
            path: root_path.clone(),
        });
        let root_request = NotPinnedRequest {
            location: root_loc,
            versions: None,
            features: root_features,
            local_root: Some(root_path.clone()),
        };
        // The action is always RequestActionFetch, so ignore retured value.
        let action = state.get_request_action(ManifestsRequest::NotPinned(root_request))?;
        if let Some(e) = action.1.into_iter().next() {
            return Err(e)
                .context_internal("Request action for the first request should never have errors");
        }
        Ok(FetchResult::NotPinned(NotPinnedResult {
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
        }))
    }

    pub async fn fetch(&self, request: ManifestsRequest) -> GathererResult<Option<FetchResult>> {
        match request {
            ManifestsRequest::Pinned(pinned_request) => {
                self.fetch_registry_pinned(pinned_request).await
            }
            ManifestsRequest::NotPinned(not_pinned_request) => {
                match not_pinned_request.location.as_ref() {
                    Location::Registry { .. } => {
                        self.fetch_registry_not_pinned(not_pinned_request).await
                    }
                    Location::Git { .. } => self.fetch_git(not_pinned_request).await,
                    Location::Local { .. } => self.fetch_local(not_pinned_request),
                }
            }
        }
    }

    async fn fetch_registry_pinned(
        &self,
        request: PinnedRequest,
    ) -> GathererResult<Option<FetchResult>> {
        let Location::Registry { url, real_name } = request.package.location.as_ref() else {
            qp_bail_internal!("Tried to make pinned registry fetch for a non-registry location");
        };
        let Some(version) = request.package.version else {
            qp_bail_internal!("Tried to make pinned fetch without specifying version")
        };
        let pkg_to_fetch = PackageWithUrl {
            id: *real_name,
            version,
            url: url.clone(),
        };
        let fetcher_response: GathererComputation<Option<registry::Manifest>> = self
            .fetcher
            .get_package_metadata(&pkg_to_fetch)
            .await
            .into();
        let Some(fetcher_response) = fetcher_response.0 else {
            return Ok(GathererComputation(None, fetcher_response.1));
        };
        let expanded_loc = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: *real_name,
        });
        let manifest: QuackResult<Manifest> = fetcher_response.try_into();
        match manifest {
            Ok(manifest) => Ok(GathererComputation::only_success(Some(
                FetchResult::Pinned(PinnedResult {
                    origin_package: request.package,
                    expanded_package: ExpandedPackage {
                        location: expanded_loc,
                        version: Some(version),
                    },
                    fetched_manifest: Box::new(manifest),
                }),
            ))),
            Err(e) => Ok(GathererComputation::only_error(e)),
        }
    }

    async fn fetch_registry_not_pinned(
        &self,
        request: NotPinnedRequest,
    ) -> GathererResult<Option<FetchResult>> {
        let Location::Registry { url, real_name } = request.location.as_ref() else {
            qp_bail_internal!(
                "Tried to make not pinned registry fetch for a non-registry location"
            );
        };
        let fetcher_response: GathererComputation<Option<MultiMetadata>> = self
            .fetcher
            .get_package_all_metadata(url, *real_name)
            .await
            .into();
        let Some(fetcher_response) = fetcher_response.0 else {
            return Ok(GathererComputation(None, fetcher_response.1));
        };
        let expanded_loc = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: url.clone(),
            real_name: *real_name,
        });
        let mut fetch_result = NotPinnedResult {
            origin_location: request.location,
            fetched_manifests: HashMap::new(),
        };
        let mut errors = vec![];
        for manifest in fetcher_response.packages_metadata {
            let manifest: QuackResult<Manifest> = manifest.try_into();
            match manifest {
                Ok(manifest) => {
                    fetch_result.fetched_manifests.insert(
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
        Ok(GathererComputation(
            Some(FetchResult::NotPinned(fetch_result)),
            errors,
        ))
    }

    async fn fetch_git(&self, request: NotPinnedRequest) -> GathererResult<Option<FetchResult>> {
        let Location::Git {
            url,
            branch_or_tag,
            rev,
        } = request.location.as_ref()
        else {
            qp_bail_internal!("Tried to make git fetch for a non-git location");
        };
        let git_source = Git::new(url.clone(), *branch_or_tag, *rev);
        let fetcher_response: GathererComputation<Option<(GitCloneResponse, TempDir)>> =
            self.fetcher.clone_from_git(&git_source).await.into();
        let Some((cloned_pkg, path_where_cloned)) = fetcher_response.0 else {
            return Ok(GathererComputation(None, fetcher_response.1));
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
            return Ok(GathererComputation(None, vec![e]));
        }
        let expanded_pkg = ExpandedPackage {
            location: expanded_loc,
            version: None,
        };
        Ok(GathererComputation::only_success(Some(
            FetchResult::NotPinned(NotPinnedResult {
                origin_location: request.location,
                fetched_manifests: HashMap::from([(
                    expanded_pkg,
                    Box::new(cloned_pkg.package.manifest().clone()),
                )]),
            }),
        )))
    }

    fn fetch_local(&self, request: NotPinnedRequest) -> GathererResult<Option<FetchResult>> {
        let Location::Local { .. } = request.location.as_ref() else {
            qp_bail_internal!("Tried to make local fetch for a non-local location")
        };
        let Some(local_root) = request.local_root else {
            qp_bail_internal!("Tried to make local fetch with unknown local root")
        };
        let pkg_ctx = PackageLoader::find_at_exact_directory(&local_root, self.ctx);

        match pkg_ctx {
            Ok(pkg_ctx) => {
                let exp_pkg = ExpandedPackage {
                    location: InternedExpandedLocation::new(ExpandedLocation::Local {
                        absolute_path: local_root,
                    }),
                    version: None,
                };
                Ok(GathererComputation::only_success(Some(
                    FetchResult::NotPinned(NotPinnedResult {
                        origin_location: request.location,
                        fetched_manifests: HashMap::from([(
                            exp_pkg,
                            Box::new(pkg_ctx.package().manifest().clone()),
                        )]),
                    }),
                )))
            }
            Err(e) => Ok(GathererComputation::only_error(e)),
        }
    }
}
