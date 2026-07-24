use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};

use tracing::debug;

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
use crate::quackpack::schemas::registry;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_path_buf::ToPathBuf;
use crate::quackpack::util::to_url::ToUrl;
use crate::util::error::{ErrorsLogger, MessageError};
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
        let mut errors = ErrorsLogger::default();
        let root_fetch_result = self.fetch_root(
            root_path,
            root_manifest,
            root_features,
            &mut state,
            &mut errors,
        )?;

        let mut fetches = vec![];
        let mut requests = state.handle_fetch_response(root_fetch_result, &mut errors)?;
        while !fetches.is_empty() || !requests.is_empty() {
            if let Some(request) = requests.pop() {
                let action = state.get_request_action(request.clone(), &mut errors)?;
                match action {
                    RequestAction::Fetch => fetches.push(request),
                    RequestAction::More {
                        requests: new_requests,
                    } => requests.extend(new_requests),
                }
            }
            if !fetches.is_empty() {
                let request = fetches.remove(0);
                let response = self.fetch(request, &mut errors)?;
                requests.extend(state.handle_fetch_response(response, &mut errors)?);
            }
        }
        if !errors.is_empty() {
            if mode.suppress_foreign_manifests_errors {
                for e in errors {
                    self.fetcher.ctx().error_console().info_verbose(format!(
                        "error\n{e}\nsuppressed due to the Merciful mode of the solver",
                    ))?;
                }
            } else {
                return Err(errors.unwrap_first());
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
        errors: &mut ErrorsLogger,
    ) -> QuackResult<FetchResponse> {
        let root_name = root_manifest.name();
        let root_version = root_manifest.version();
        if cfg!(debug_assertions) {
            assert_root_features_are_expanded(&root_manifest, &root_features);
        }
        let root_url = root_path
            .to_url()
            .context_internal("failed to generate url from a path")?;
        let root_source = Source::for_local_with_url(root_url);
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
                            FullOrigin::for_local(&root_path)
                                .context_internal("failed to translate root path into url")?,
                        ),
                        root_version,
                    ),
                    Box::new(root_manifest),
                )]),
            },
        )))
    }

    /// Helper for [`Gatherer::explore()`], performs a fetch.
    #[tracing::instrument(skip_all)]
    pub fn fetch(
        &mut self,
        request: ManifestsRequest,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<FetchResponse> {
        debug!(?request);
        match request {
            ManifestsRequest::Pinned(pinned_request) => {
                self.fetch_registry_pinned(pinned_request, errors)
            }
            ManifestsRequest::NotPinned(not_pinned_request) => {
                match not_pinned_request.id.source.kind() {
                    SourceKind::Registry => {
                        let url = not_pinned_request.id.source.url();
                        Ok(self.fetch_registry_not_pinned(
                            &not_pinned_request,
                            url,
                            not_pinned_request.id.name,
                            errors,
                        ))
                    }
                    SourceKind::Git(git_ref) => {
                        let url = not_pinned_request.id.source.url();
                        Ok(self.fetch_git(&not_pinned_request, url, *git_ref, errors))
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
    fn fetch_registry_pinned(
        &mut self,
        request: PinnedRequest,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<FetchResponse> {
        debug!("fetching registry (pinned)");
        if !matches!(request.id.source.kind(), SourceKind::Registry) {
            qp_bail_internal!("tried to make pinned registry fetch for a non-registry source");
        };
        let pkg_to_fetch = PackageWithUrl {
            name: request.id.name,
            version: request.version,
            url: request.id.source.url(),
        };
        let fetcher_response = match self.fetcher.get_package_metadata(&pkg_to_fetch) {
            Ok(response) => response,
            Err(e) => {
                errors.log(e);
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
        match <registry::Manifest as TryInto<Manifest>>::try_into(registry_manifest) {
            Ok(manifest) => Ok(FetchResponse::Success(FetchSuccess::Pinned(
                PinnedSuccess {
                    origin_id: request.id,
                    origin_version: request.version,
                    answer_package: PackageId::new(answer_identity, manifest.version()),
                    fetched_manifest: Box::new(manifest),
                },
            ))),
            Err(e) => {
                debug!("failed to fetch: {e}");
                errors.log(e);
                Ok(FetchResponse::failed_pinned(request.id, request.version))
            }
        }
    }

    /// Helper for [`Gatherer::explore()`], performs a not pinned registry fetch
    /// (registry fetch of all the versions of some package).
    fn fetch_registry_not_pinned(
        &mut self,
        request: &NotPinnedRequest,
        url: InternedUrl,
        real_name: StrId,
        errors: &mut ErrorsLogger,
    ) -> FetchResponse {
        debug!("fetching registry (not pinned)");
        let fetcher_response = match self.fetcher.get_package_all_metadata(url, real_name) {
            Ok(response) => response,
            Err(e) => {
                errors.log(e);
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
            let manifest: QuackResult<Manifest> = manifest
                .try_into()
                .with_context(|| {
                    format!("registry `{url}` has responded with an invalid JSON for the package multimetadata of `{real_name}` version `{version}")
                });
            match manifest {
                Ok(manifest) => {
                    fetch_response.fetched_manifests.insert(
                        PackageId::new(answer_identity, manifest.version()),
                        Box::new(manifest),
                    );
                }
                Err(e) => {
                    debug!("failed to fetch: {e}");
                    errors.log(e);
                }
            }
        }
        FetchResponse::Success(FetchSuccess::NotPinned(fetch_response))
    }

    /// Helper for [`Gatherer::explore()`], performs a git fetch
    /// (fetch from an external git repository).
    fn fetch_git(
        &mut self,
        request: &NotPinnedRequest,
        url: InternedUrl,
        reference: GitReference,
        errors: &mut ErrorsLogger,
    ) -> FetchResponse {
        debug!("fetching git");
        // We create a temporary logger to check if `try_get_cached_git` produced any errors.
        let mut tmp_logger = ErrorsLogger::default();
        let cached_git = self.try_get_cached_git(request, url, reference, &mut tmp_logger);
        if !tmp_logger.is_empty() {
            let err = tmp_logger.unwrap_first();
            errors.log(err.context(MessageError::new("when trying to get cached git")));
            return FetchResponse::failed_not_pinned(request.id);
        }
        if let Some(success) = cached_git {
            debug!("git request was cached");
            return FetchResponse::Success(FetchSuccess::NotPinned(success));
        }
        if self.fetcher.ctx().is_offline() {
            return FetchResponse::failed_not_pinned(request.id);
        }

        let (cloned_pkg, path_where_cloned) = match self.fetcher.clone_from_git(&url, reference) {
            Ok(response) => response,
            Err(e) => {
                errors.log(e);
                return FetchResponse::failed_not_pinned(request.id);
            }
        };
        let answer_identity = FullIdentity::new(
            request.id.name,
            FullOrigin::for_git(url, cloned_pkg.commit_hash),
        );
        if !self.git_access.is_stored(url, &cloned_pkg.commit_hash)
            && let Err(e) =
                self.git_access
                    .store(url, &cloned_pkg.commit_hash, path_where_cloned.path())
        {
            debug!("failed to store a new git: {e}");
            errors.log(e);
            return FetchResponse::failed_not_pinned(request.id);
        }
        let answer_pkg = PackageId::new(answer_identity, cloned_pkg.package.manifest().version());
        FetchResponse::Success(FetchSuccess::NotPinned(NotPinnedSuccess {
            origin_id: request.id,
            fetched_manifests: HashMap::from([(
                answer_pkg,
                Box::new(cloned_pkg.package.manifest().clone()),
            )]),
        }))
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
            let mut dummy_logger = ErrorsLogger::default();
            let stored_git_manifest =
                self.fetch_local(&storage_local_request, &path, &mut dummy_logger);
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

    /// Helper for [`Gatherer::explore()`], performs a local fetch
    /// (fetch from a given path).
    fn fetch_local(
        &self,
        request: &NotPinnedRequest,
        path: &Path,
        errors: &mut ErrorsLogger,
    ) -> FetchResponse {
        let pcx = match PackageLoader::find_at_exact_directory(path, self.fetcher.ctx()) {
            Ok(pcx) => pcx,
            Err(e) => {
                errors.log(e);
                return FetchResponse::failed_not_pinned(request.id);
            }
        };
        let root = pcx.package().root();
        let Ok(answer_origin) = FullOrigin::for_local(root) else {
            return FetchResponse::failed_not_pinned(request.id);
        };
        let answer_identity = FullIdentity::new(request.id.name, answer_origin);
        FetchResponse::Success(FetchSuccess::NotPinned(NotPinnedSuccess {
            origin_id: request.id,
            fetched_manifests: HashMap::from([(
                PackageId::new(answer_identity, pcx.package().version()),
                Box::new(pcx.package().manifest().clone()),
            )]),
        }))
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
