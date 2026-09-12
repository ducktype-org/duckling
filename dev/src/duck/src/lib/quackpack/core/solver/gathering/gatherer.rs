use std::cell::RefCell;
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};
use std::pin::Pin;

use futures::StreamExt;
use futures::stream::FuturesUnordered;
use tracing::{debug, error, trace};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::fetcher::types::{FetcherResponse, PackageWithUrl};
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::gathering::fetch_types::{
    FetchResponse, FetchSuccess, ManifestsRequest, NotPinnedRequest, NotPinnedSuccess,
    PinnedRequest, PinnedSuccess, RequestAction, RequestIdentifier,
};
use crate::quackpack::core::solver::gathering::gatherer_state::{GatheredInfo, GathererState};
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::{
    FeatureName, GitReference, Manifest, PackageId, PackageLoader, Source, SourceKind,
};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_path_buf::ToPathBuf;
use crate::quackpack::util::to_url::ToUrl;
use crate::util::error::{ErrorsLogger, MessageError};
use crate::{
    DuckContext, QuackError, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal,
    qp_err,
};

type FetchFuture<'a> = Pin<Box<dyn Future<Output = QuackResult<FetchResponse>> + 'a>>;

/// A struct for fetching manifests for all the packages potentially used in the dependency resolution.
pub struct Gatherer<'duck, 'a, Access: GitAccess> {
    fetcher: &'a Fetcher<'duck>,
    git_access: &'a Access,
}

impl<'duck, 'a, Access: GitAccess> Gatherer<'duck, 'a, Access> {
    /// Creates a new, empty [`Gatherer`].
    pub fn new(fetcher: &'a Fetcher<'duck>, git_access: &'a Access) -> Self {
        Self {
            fetcher,
            git_access,
        }
    }

    /// Main entry point, explores the dependency graph of the root package in a BFS-like manner.
    /// For a given dependency entry in a manifest, fetches the manifests of the potential realizations
    /// and repeats the process for their manifests.
    #[tracing::instrument(skip_all, fields(mode, offline = self.fetcher.ctx().is_offline()))]
    pub async fn explore(
        &self,
        root_path: PathBuf,
        root_manifest: Box<Manifest>,
        root_features: HashSet<FeatureName>,
        mode: SolverMode,
    ) -> QuackResult<GatheredInfo> {
        let mut state = GathererState::default();
        let errors = RefCell::new(ErrorsLogger::default());
        let root_fetch_result = self.fetch_root(
            root_path,
            root_manifest,
            root_features,
            &mut state,
            &mut errors.borrow_mut(),
        )?;

        self.explore_deps(root_fetch_result, &mut state, mode, errors)
            .await?;
        state.try_into()
    }

    #[tracing::instrument(skip_all, fields(mode, offline = self.fetcher.ctx().is_offline()))]
    async fn explore_deps(
        &self,
        root_fetch_response: FetchResponse,
        state: &mut GathererState,
        mode: SolverMode,
        logger: RefCell<ErrorsLogger>,
    ) -> QuackResult<()> {
        let mut fetches = FuturesUnordered::new();
        let requests =
            state.handle_fetch_response(root_fetch_response, &mut logger.borrow_mut())?;
        self.recursively_push_requests(&fetches, requests, state, &logger)?;
        while let Some(response) = fetches.next().await {
            let response = response?;
            let new_requests = state.handle_fetch_response(response, &mut logger.borrow_mut())?;
            self.recursively_push_requests(&fetches, new_requests, state, &logger)?;
        }
        if !logger.borrow().is_empty() {
            if mode.suppress_foreign_manifests_errors {
                for e in logger.take() {
                    self.fetcher.ctx().info_verbose(format!(
                        "error\n{e}\nsuppressed due to the Merciful mode of the solver",
                    ))?;
                }
            } else {
                return Err(logger.take().unwrap_first());
            }
        }
        Ok(())
    }

    /// Push `requests` recursively to `fetches`.
    ///
    /// Any [`RequestAction::More`] turns into a recursive call.
    ///
    /// We process [`ManifestsRequest`]s until only [`RequestAction::Fetch`]es are left.
    fn recursively_push_requests<'b>(
        &'b self,
        fetches: &FuturesUnordered<FetchFuture<'b>>,
        requests: Vec<ManifestsRequest>,
        state: &mut GathererState,
        errors: &'b RefCell<ErrorsLogger>,
    ) -> QuackResult<()> {
        for request in requests {
            let action = state.get_request_action(request.clone(), &mut errors.borrow_mut())?;
            match action {
                RequestAction::Fetch => fetches.push(Box::pin(self.fetch(request, errors))),
                RequestAction::More { requests } => {
                    self.recursively_push_requests(fetches, requests, state, errors)?
                }
            }
        }
        Ok(())
    }

    /// Helper for [`Gatherer::explore()`], creates a dummy [`ManifestsRequest`] for the root package to update the state
    /// and returns a dummy [`FetchResponse`], to create a starting point for the [`Gatherer::explore()`] function.
    ///
    /// Note: We assume that `root_features` are expanded.
    #[tracing::instrument(skip_all, fields(root_path))]
    fn fetch_root(
        &self,
        root_path: PathBuf,
        root_manifest: Box<Manifest>,
        root_features: HashSet<FeatureName>,
        state: &mut GathererState,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<FetchResponse> {
        let root_name = root_manifest.name();
        let root_version = root_manifest.version();
        if cfg!(debug_assertions) {
            assert_root_features_are_expanded(&root_manifest, &root_features);
        }
        let root_url = root_path.to_url().with_context_internal(|| {
            format!("failed to generate url from a path `{root_path:?}`")
        })?;
        let root_source = Source::for_local_with_url(root_url);
        if does_package_depend_on_itself(&[root_source], &root_manifest) {
            return Err(self_dependent_root_package_error());
        }
        let root_request = NotPinnedRequest {
            id: RequestIdentifier {
                name: root_name,
                source: root_source,
            },
            versions: None,
            features: root_features,
        };
        // The action is always RequestActionFetch, so ignore returned value.
        let _ = state.get_request_action(ManifestsRequest::NotPinned(root_request), errors)?;
        Ok(FetchResponse::Success(FetchSuccess::NotPinned(
            NotPinnedSuccess {
                origin_id: RequestIdentifier {
                    name: root_name,
                    source: root_source,
                },
                fetched_manifests: HashMap::from([(
                    PackageId::new(
                        FullIdentity::new(
                            root_name,
                            FullOrigin::for_local(&root_path).with_context_internal(|| {
                                format!("failed to translate root path into url `{root_path:?}`")
                            })?,
                        ),
                        root_version,
                    ),
                    root_manifest,
                )]),
            },
        )))
    }

    /// Helper for [`Gatherer::explore()`], performs a fetch.
    ///
    /// After performing the fetch we check that the resulting manifests satisfy the requests.
    /// For registry fetches we also check that repository dependencies do not have local dependencies.
    #[tracing::instrument(skip_all)]
    pub async fn fetch(
        &self,
        request: ManifestsRequest,
        errors: &RefCell<ErrorsLogger>,
    ) -> QuackResult<FetchResponse> {
        debug!(?request);
        match request {
            ManifestsRequest::Pinned(pinned_request) => {
                self.fetch_registry_pinned(pinned_request, errors).await
            }
            ManifestsRequest::NotPinned(not_pinned_request) => {
                match not_pinned_request.id.source.kind() {
                    SourceKind::Registry => {
                        let url = not_pinned_request.id.source.url();
                        Ok(self
                            .fetch_registry_not_pinned(
                                &not_pinned_request,
                                url,
                                not_pinned_request.id.name,
                                errors,
                            )
                            .await)
                    }
                    SourceKind::Git(git_ref) => {
                        let url = not_pinned_request.id.source.url();
                        self.fetch_git(&not_pinned_request, url, git_ref, errors)
                            .await
                    }
                    SourceKind::Local => {
                        let path_url = not_pinned_request.id.source.url();
                        let path = path_url.to_path_buf()?;
                        Ok(self.fetch_local(&not_pinned_request, &path, errors))
                    }
                }
            }
        }
    }

    /// Helper for [`Gatherer::explore()`], performs a pinned registry fetch
    /// (registry fetch with a specified version).
    async fn fetch_registry_pinned(
        &self,
        request: PinnedRequest,
        errors: &RefCell<ErrorsLogger>,
    ) -> QuackResult<FetchResponse> {
        trace!("fetching registry (pinned)");
        if !matches!(request.id.source.kind(), SourceKind::Registry) {
            qp_bail_internal!(
                "tried to make pinned registry fetch for a non-registry source: {request:#?}"
            );
        };
        let pkg_to_fetch = PackageWithUrl {
            name: request.id.name,
            version: request.version,
            url: request.id.source.url(),
        };
        let fetcher_response = match self.fetcher.get_package_metadata(pkg_to_fetch).await {
            Ok(response) => response,
            Err(e) => {
                errors.borrow_mut().log(e);
                return Ok(FetchResponse::failed_pinned(request.id, request.version));
            }
        };
        let answer_identity = FullIdentity::new(
            request.id.name,
            FullOrigin::for_registry(request.id.source.url()),
        );
        let FetcherResponse::Some(registry_manifest) = fetcher_response else {
            return Ok(FetchResponse::failed_pinned(request.id, request.version));
        };
        let manifest: QuackResult<Manifest> = (registry_manifest, self.fetcher.ctx()).try_into();
        match manifest
            .and_then(|manifest| manifest.bail_if_incoherent_with_request_pinned(&request))
        {
            Ok(manifest) => Ok(FetchResponse::Success(FetchSuccess::Pinned(
                PinnedSuccess {
                    origin_id: request.id,
                    origin_version: request.version,
                    answer_package: PackageId::new(answer_identity, manifest.version()),
                    fetched_manifest: Box::new(manifest),
                },
            ))),
            Err(e) => {
                error!(error = %e, "failed to fetch");
                errors.borrow_mut().log(e);
                Ok(FetchResponse::failed_pinned(request.id, request.version))
            }
        }
    }

    /// Helper for [`Gatherer::explore()`], performs a not pinned registry fetch
    /// (registry fetch of all the versions of some package).
    /// If the response would be empty (would contain no manifests), logs an error.
    async fn fetch_registry_not_pinned(
        &self,
        request: &NotPinnedRequest,
        url: InternedUrl,
        real_name: StrId,
        errors: &RefCell<ErrorsLogger>,
    ) -> FetchResponse {
        trace!("fetching registry (not pinned)");
        let fetcher_response = match self.fetcher.get_package_all_metadata(url, real_name).await {
            Ok(response) => response,
            Err(e) => {
                errors.borrow_mut().log(e);
                return FetchResponse::failed_not_pinned(request.id);
            }
        };
        let FetcherResponse::Some(fetcher_response) = fetcher_response else {
            return FetchResponse::failed_not_pinned(request.id);
        };
        let answer_identity = FullIdentity::new(real_name, FullOrigin::for_registry(url));
        let mut fetch_response = NotPinnedSuccess {
            origin_id: request.id,
            fetched_manifests: HashMap::new(),
        };
        for manifest in fetcher_response.packages_metadata {
            let version = manifest.metadata.version;
            let manifest: QuackResult<Manifest> = (manifest, self.fetcher.ctx()).try_into()
                .with_context(|| {
                    format!("registry `{url}` has responded with an invalid JSON for the package multimetadata of `{real_name}` version `{version}")
                });
            match manifest
                .and_then(|manifest| manifest.bail_if_incoherent_with_request_not_pinned(request))
            {
                Ok(manifest) => {
                    fetch_response.fetched_manifests.insert(
                        PackageId::new(answer_identity, manifest.version()),
                        Box::new(manifest),
                    );
                }
                Err(e) => {
                    // We log errors and filter only good manifests.
                    error!(error = %e, "failed to fetch");
                    errors.borrow_mut().log(e);
                }
            }
        }
        if fetch_response.fetched_manifests.is_empty() {
            let err = qp_err!("no packages found satisfying the request for {real_name}");
            errors.borrow_mut().log(err);
            return FetchResponse::failed_not_pinned(request.id);
        }
        FetchResponse::Success(FetchSuccess::NotPinned(fetch_response))
    }

    /// Helper for [`Gatherer::explore()`], performs a git fetch
    /// (fetch from an external git repository).
    #[tracing::instrument(skip_all, fields(?request, %url, ?reference))]
    async fn fetch_git(
        &self,
        request: &NotPinnedRequest,
        url: InternedUrl,
        reference: GitReference,
        errors: &RefCell<ErrorsLogger>,
    ) -> QuackResult<FetchResponse> {
        trace!("fetching git");
        // We create a temporary logger to check if `try_get_cached_git` produced any errors.
        let mut cache_logger = ErrorsLogger::default();
        if let Some(cached_git) =
            self.try_get_cached_git(request, url, reference, &mut cache_logger)
        {
            debug!("git request was cached");
            return Ok(FetchResponse::Success(FetchSuccess::NotPinned(cached_git)));
        }
        if !cache_logger.is_empty() {
            let err = cache_logger.unwrap_first();
            errors.borrow_mut().log(err.context(MessageError::new(
                "when trying to get cached git or during fastpath",
            )));
            return Ok(FetchResponse::failed_not_pinned(request.id));
        }
        match self.try_git_fastpath(request, url, reference).await {
            Err(e) => {
                error!(error = %e, "fast path failed");
                // We swallow errors on git fast path as this is a general way of handling them in all of the codebase,
                // as it is well ... a fast path.
                display_git_fast_path_failure_warning(&e, request, self.fetcher.ctx())?;
            }
            Ok(Some(fast_path_git)) => {
                debug!("git fast path worked");
                return Ok(FetchResponse::Success(FetchSuccess::NotPinned(
                    fast_path_git,
                )));
            }
            Ok(None) => {}
        };
        if self.fetcher.ctx().is_offline() {
            return Ok(FetchResponse::failed_not_pinned(request.id));
        }

        let (cloned_pkg, path_where_cloned) = match self.fetcher.clone_from_git(&url, reference) {
            Ok(response) => response,
            Err(e) => {
                errors.borrow_mut().log(e);
                return Ok(FetchResponse::failed_not_pinned(request.id));
            }
        };
        let manifest = match cloned_pkg
            .package
            .into_manifest()
            .bail_if_incoherent_with_request_not_pinned(request)
        {
            Ok(manifest) => manifest,
            Err(e) => {
                errors.borrow_mut().log(e);
                return Ok(FetchResponse::failed_not_pinned(request.id));
            }
        };
        let sources: &[Source] =
            if let Ok(path_source) = Source::for_local(path_where_cloned.path()) {
                &[path_source, request.id.source]
            } else {
                &[request.id.source]
            };
        if does_package_depend_on_itself(sources, &manifest) {
            errors
                .borrow_mut()
                .log(self_dependent_dependency_error(request.id));
            return Ok(FetchResponse::failed_not_pinned(request.id));
        }
        let answer_identity = FullIdentity::new(
            request.id.name,
            FullOrigin::for_git(url, cloned_pkg.commit_hash),
        );
        if !self.git_access.is_stored(url, &cloned_pkg.commit_hash)
            && let Err(e) =
                self.git_access
                    .store(url, &cloned_pkg.commit_hash, path_where_cloned.path())
        {
            error!(error = %e, "failed to store a new git");
            errors.borrow_mut().log(e);
            return Ok(FetchResponse::failed_not_pinned(request.id));
        }
        let answer_pkg = PackageId::new(answer_identity, manifest.version());
        Ok(FetchResponse::Success(FetchSuccess::NotPinned(
            NotPinnedSuccess {
                origin_id: request.id,
                fetched_manifests: HashMap::from([(answer_pkg, Box::new(manifest))]),
            },
        )))
    }

    /// Helper for [`Gatherer::fetch_git`].
    /// Searches for cached git satisfying the given request and loads the found package with [`PackageLoader`].
    /// If it cannot find a cached git, returns [`None`].
    fn try_get_cached_git(
        &self,
        request: &NotPinnedRequest,
        url: InternedUrl,
        reference: GitReference,
        errors: &mut ErrorsLogger,
    ) -> Option<NotPinnedSuccess> {
        if let GitReference::Rev(commit) = reference
            && let Some(path) = self.git_access.path_if_stored(url, &commit)
        {
            let answer_identity =
                FullIdentity::new(request.id.name, FullOrigin::for_git(url, commit));
            let source = match Source::for_local(&path) {
                Ok(source) => source,
                Err(e) => {
                    errors.log(e);
                    return None;
                }
            };
            let storage_local_request = NotPinnedRequest {
                id: RequestIdentifier {
                    name: request.id.name,
                    source,
                },
                versions: None,
                features: [].into(),
            };
            // We create a dummy logger, because we do not care about errors of `fetch_local`,
            // if it returns a `FetchResponse::Success` then there were no errors anyway.
            let dummy_logger = RefCell::new(ErrorsLogger::default());
            let stored_git_manifest =
                self.fetch_local(&storage_local_request, &path, &dummy_logger);
            if let FetchResponse::Success(FetchSuccess::NotPinned(success)) = stored_git_manifest
                && let Some(manifest) = success.fetched_manifests.into_values().next()
            {
                let pkg = PackageId::new(answer_identity, manifest.version());
                return Some(NotPinnedSuccess {
                    origin_id: request.id,
                    fetched_manifests: [(pkg, manifest)].into(),
                });
            }
        }
        None
    }

    /// Tries to use git fast path, to get the manifests without performing clone.
    async fn try_git_fastpath(
        &self,
        request: &NotPinnedRequest,
        url: InternedUrl,
        reference: GitReference,
    ) -> QuackResult<Option<NotPinnedSuccess>> {
        let Some(fast_path_client) = self.fetcher.try_get_fastpath(url) else {
            return Ok(None);
        };
        let source = Source::for_git(url, reference);
        let commit = fast_path_client.get_commit_hash(reference).await?;
        let manifest = fast_path_client.download_manifest(commit).await?;
        if does_package_depend_on_itself(&[source], &manifest) {
            return Err(self_dependent_dependency_error(request.id));
        }
        let answer_identity = FullIdentity::new(request.id.name, FullOrigin::for_git(url, commit));
        let pkg_id = PackageId::new(answer_identity, manifest.version());
        Ok(Some(NotPinnedSuccess {
            origin_id: request.id,
            fetched_manifests: [(pkg_id, Box::new(manifest))].into(),
        }))
    }

    /// Helper for [`Gatherer::explore()`], performs a local fetch
    /// (fetch from a given path).
    fn fetch_local(
        &self,
        request: &NotPinnedRequest,
        path: &Path,
        errors: &RefCell<ErrorsLogger>,
    ) -> FetchResponse {
        let pcx = match PackageLoader::find_at_exact_directory(path, self.fetcher.ctx()) {
            Ok(pcx) => pcx,
            Err(e) => {
                errors.borrow_mut().log(e);
                return FetchResponse::failed_not_pinned(request.id);
            }
        };
        if does_package_depend_on_itself(&[request.id.source], pcx.package().manifest()) {
            errors
                .borrow_mut()
                .log(self_dependent_dependency_error(request.id));
            return FetchResponse::failed_not_pinned(request.id);
        }
        let root = pcx.package().root();
        let Ok(answer_origin) = FullOrigin::for_local(root) else {
            return FetchResponse::failed_not_pinned(request.id);
        };
        let manifest = match pcx
            .into_package()
            .into_manifest()
            .bail_if_incoherent_with_request_not_pinned(request)
        {
            Ok(manifest) => manifest,
            Err(e) => {
                errors.borrow_mut().log(e);
                return FetchResponse::failed_not_pinned(request.id);
            }
        };
        let answer_identity = FullIdentity::new(request.id.name, answer_origin);
        FetchResponse::Success(FetchSuccess::NotPinned(NotPinnedSuccess {
            origin_id: request.id,
            fetched_manifests: HashMap::from([(
                PackageId::new(answer_identity, manifest.version()),
                Box::new(manifest),
            )]),
        }))
    }
}

impl Manifest {
    /// Check that the manifest's name agrees with the request.
    fn bail_if_incoherent_with_request_not_pinned(
        self,
        request: &NotPinnedRequest,
    ) -> QuackResult<Self> {
        if self.name() != request.id.name {
            qp_bail!(
                "request for all versions of {} returned package with different name",
                request.id.name
            )
        }
        Ok(self)
    }

    /// Check that the manifest's name and version agree with the request.
    fn bail_if_incoherent_with_request_pinned(self, request: &PinnedRequest) -> QuackResult<Self> {
        if self.name() != request.id.name {
            qp_bail!(
                "request for {} version {} returned package with different name",
                request.id.name,
                request.version
            )
        }
        if self.version() != request.version {
            qp_bail!(
                "request for {} version {} returned package with different version",
                request.id.name,
                request.version
            )
        }
        Ok(self)
    }
}

#[track_caller]
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

fn display_git_fast_path_failure_warning(
    error: &QuackError,
    request: &NotPinnedRequest,
    ctx: &DuckContext,
) -> QuackResult<()> {
    let identifier = request.id;
    let name = identifier.name;
    let source = identifier.source;
    ctx.warning(format!(
        "git fast path for `{name} {source}` failed: {error}"
    ))?;
    ctx.info("switching to cloning git repository")?;
    Ok(())
}

/// Check whether a given package depends on itself.
///
/// `sources` is a slice of this package's sources.
///
/// It accepts multiple sources (but in practice it's 1 or 2), because a git dependency can be
/// self-dependent either as a local dependency with a path `.`, or as a git dependency with the
/// same URL.
///
/// Local and registry dependencies don't have such struggles.
fn does_package_depend_on_itself(sources: &[Source], manifest: &Manifest) -> bool {
    manifest
        .dependencies()
        .all_dependencies()
        .iter()
        .any(|dep| sources.contains(&dep.source()))
}

fn self_dependent_root_package_error() -> QuackError {
    qp_err!("root package depends on itself")
}

fn self_dependent_dependency_error(id: RequestIdentifier) -> QuackError {
    qp_err!("dependency `{} {}` depends on itself", id.name, id.source)
}
