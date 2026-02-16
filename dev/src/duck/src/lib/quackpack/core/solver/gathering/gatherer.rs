use futures::future::select_all;
use std::{
    collections::{HashMap, HashSet},
    path::PathBuf,
    sync::Arc,
};

use tempfile::TempDir;
use tokio::sync::Mutex;

use crate::{
    QpCtx, QuackResult, StrId, qp_bail_internal,
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
                    FetchRequest, FetchResult, NotPinnedRequest, NotPinnedResult, PinnedRequest,
                    PinnedResult,
                },
                gatherer_state::{GathererState, RequestAction},
            },
            git_access::GitAccess,
            types_common::{
                ExpandedLocGit, ExpandedLocLocal, ExpandedLocRegistry, ExpandedLocation,
                ExpandedPackage, InternedExpandedLocation, InternedLocation, LocLocal, Location,
            },
        },
        schemas::registry,
    },
};

pub async fn explore<'duck>(
    ctx: &'duck QpCtx<'duck>,
    fetcher: &'duck Fetcher<'duck>,
    root_path: PathBuf,
    root_manifest: Manifest,
    root_features: HashSet<FeatureName>,
    mode: SolverMode,
    git_access: Arc<Mutex<impl GitAccess>>,
) -> QuackResult<()> {
    let mut state = GathererState::new();
    let root_loc = InternedLocation::new(Location::Local(LocLocal {
        path: root_path.clone(),
    }));
    let root_request = NotPinnedRequest {
        location: root_loc,
        versions: None,
        features: root_features,
        local_root: Some(root_path.clone()),
    };
    let mut errors = vec![];
    // The action is always RequestActionFetch, so ignore retured value.
    let _action = state
        .get_request_action(FetchRequest::NotPinned(root_request))?
        .dump_errors(&mut errors);
    let root_fetch_result = FetchResult::NotPinned(NotPinnedResult {
        origin_location: root_loc,
        fetched_manifests: HashMap::from([(
            ExpandedPackage {
                location: InternedExpandedLocation::new(ExpandedLocation::Local(
                    ExpandedLocLocal {
                        absolute_path: root_path,
                    },
                )),
                version: None,
            },
            Box::new(root_manifest),
        )]),
    });

    let mut tasks = vec![];
    let mut deps = state
        .handle_response(root_fetch_result)?
        .dump_errors(&mut errors);
    while !tasks.is_empty() || !deps.is_empty() {
        if let Some(request) = deps.pop() {
            let action = state
                .get_request_action(request.clone())?
                .dump_errors(&mut errors);
            match action {
                RequestAction::Fetch => {
                    tasks.push(Box::pin(fetch(ctx, fetcher, request, git_access.clone())))
                }
                RequestAction::More { requests } => deps.extend(requests),
            }
        }
        if !tasks.is_empty() {
            let (fetch_result, _, remaining) = select_all(tasks).await;
            if let Some(fetch_result) = fetch_result?.dump_errors(&mut errors) {
                deps.extend(
                    state
                        .handle_response(fetch_result)?
                        .dump_errors(&mut errors),
                );
            }
            tasks = remaining;
        }
    }
    if !errors.is_empty() {
        match mode {
            SolverMode::Merciful => {
                for e in errors {
                    ctx.error_console().info(format!(
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

async fn fetch<'duck>(
    ctx: &'duck QpCtx<'duck>,
    fetcher: &'duck Fetcher<'duck>,
    request: FetchRequest,
    git_access: Arc<Mutex<impl GitAccess>>,
) -> GathererResult<Option<FetchResult>> {
    match request {
        FetchRequest::Pinned(pinned_request) => {
            fetch_registry_pinned(fetcher, pinned_request).await
        }
        FetchRequest::NotPinned(not_pinned_request) => match not_pinned_request.location.as_ref() {
            Location::Registry(_) => fetch_registry_not_pinned(fetcher, not_pinned_request).await,
            Location::Git(_) => fetch_git(fetcher, not_pinned_request, git_access).await,
            Location::Local(_) => fetch_local(ctx, not_pinned_request),
        },
    }
}

async fn fetch_registry_pinned<'duck>(
    fetcher: &'duck Fetcher<'duck>,
    request: PinnedRequest,
) -> GathererResult<Option<FetchResult>> {
    let Location::Registry(loc_registry) = request.package.location.as_ref() else {
        qp_bail_internal!("Tried to make pinned registry fetch for a non-registry location");
    };
    let Some(version) = request.package.version else {
        qp_bail_internal!("Tried to make pinned fetch without specifying version")
    };
    let pkg_to_fetch = PackageWithUrl {
        id: loc_registry.real_name,
        version,
        url: loc_registry.url.clone(),
    };
    let fetcher_response: GathererComputation<Option<registry::Manifest>> =
        fetcher.get_package_metadata(&pkg_to_fetch).await.into();
    let Some(fetcher_response) = fetcher_response.0 else {
        return Ok(GathererComputation(None, fetcher_response.1));
    };
    let expanded_loc =
        InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
            url: loc_registry.url.clone(),
            real_name: loc_registry.real_name,
        }));
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

async fn fetch_registry_not_pinned<'duck>(
    fetcher: &'duck Fetcher<'duck>,
    request: NotPinnedRequest,
) -> GathererResult<Option<FetchResult>> {
    let Location::Registry(loc_registry) = request.location.as_ref() else {
        qp_bail_internal!("Tried to make not pinned registry fetch for a non-registry location");
    };
    let fetcher_response: GathererComputation<Option<MultiMetadata>> = fetcher
        .get_package_all_metadata(&loc_registry.url, loc_registry.real_name)
        .await
        .into();
    let Some(fetcher_response) = fetcher_response.0 else {
        return Ok(GathererComputation(None, fetcher_response.1));
    };
    let expanded_loc =
        InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
            url: loc_registry.url.clone(),
            real_name: loc_registry.real_name,
        }));
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

async fn fetch_git<'duck>(
    fetcher: &'duck Fetcher<'duck>,
    request: NotPinnedRequest,
    git_access: Arc<Mutex<impl GitAccess>>,
) -> GathererResult<Option<FetchResult>> {
    let Location::Git(loc_git) = request.location.as_ref() else {
        qp_bail_internal!("Tried to make git fetch for a non-git location");
    };
    let git_source = Git::new(loc_git.url.clone(), loc_git.branch_or_tag, loc_git.rev);
    let fetcher_response: GathererComputation<Option<(GitCloneResponse, TempDir)>> =
        fetcher.clone_from_git(&git_source).await.into();
    let Some((cloned_pkg, path_where_cloned)) = fetcher_response.0 else {
        return Ok(GathererComputation(None, fetcher_response.1));
    };
    let url = StrId::new(loc_git.url.clone());
    let expanded_loc = InternedExpandedLocation::new(ExpandedLocation::Git(ExpandedLocGit {
        url,
        commit: cloned_pkg.commit_hash,
    }));
    let mut git_access = git_access.lock().await;
    if !git_access.is_stored(url, cloned_pkg.commit_hash) {
        git_access.store(url, cloned_pkg.commit_hash, path_where_cloned.path());
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

fn fetch_local<'duck>(
    ctx: &'duck QpCtx<'duck>,
    request: NotPinnedRequest,
) -> GathererResult<Option<FetchResult>> {
    let Location::Local(_) = request.location.as_ref() else {
        qp_bail_internal!("Tried to make local fetch for a non-local location")
    };
    let Some(local_root) = request.local_root else {
        qp_bail_internal!("Tried to make local fetch with unknown local root")
    };
    let pkg_ctx = PackageLoader::find_at_exact_directory(&local_root, ctx);

    match pkg_ctx {
        Ok(pkg_ctx) => {
            let exp_pkg = ExpandedPackage {
                location: InternedExpandedLocation::new(ExpandedLocation::Local(
                    ExpandedLocLocal {
                        absolute_path: local_root,
                    },
                )),
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
