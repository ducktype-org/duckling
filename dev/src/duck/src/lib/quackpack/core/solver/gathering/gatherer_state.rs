use std::borrow::Cow;
use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::gathering::error_suppression::{
    GathererComputation, GathererResult,
};
use crate::quackpack::core::solver::gathering::fetch_types::{
    FetchFailure, FetchResponse, FetchSuccess, ManifestsRequest, NotPinnedFailure,
    NotPinnedRequest, NotPinnedSuccess, PinnedFailure, PinnedRequest, PinnedSuccess,
    RequestIdentifier,
};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{FeatureName, Manifest, Source, Version};
use crate::quackpack::util::str_id::QpJoin;
use crate::quackpack::util::with_version::WithVersion;
use crate::util::error::MessageError;
use crate::util::extend::QpExtend;
use crate::{QuackError, QuackResult, QuackResultContext, qp_bail_internal, qp_err, qp_internal};

/// Gathered information about a particular package.
#[derive(Debug)]
pub struct PackageData {
    pub manifest: Box<Manifest>,
    pub requested_features: HashSet<FeatureName>,
    pub referenced_by_requests: bool,
}

impl PackageData {
    /// Returns a list of requests for possible realizations of package's dependencies,
    /// given already requested features of the package.
    fn dep_requests(&self) -> GathererResult<Vec<ManifestsRequest>> {
        if !self.referenced_by_requests {
            return Ok(GathererComputation::only_success(vec![]));
        }
        let mut result: GathererComputation<Vec<ManifestsRequest>> = GathererComputation::empty();
        for dependency in self.manifest.dependencies().all_dependencies() {
            if !dependency.is_enabled_for(self.requested_features.iter().copied()) {
                continue;
            }
            let source = dependency.source();
            let features: HashSet<FeatureName> = HashSet::from_iter(
                dependency.enabled_features(self.requested_features.iter().copied()),
            );
            if dependency.is_pinned() {
                let version = dependency
                    .versions()
                    .first()
                    .copied()
                    .context_internal("pinned dependency without version")?;
                result.0.push(ManifestsRequest::Pinned(PinnedRequest {
                    id: RequestIdentifier {
                        source: *source,
                        name: dependency.name(),
                    },
                    version,
                    features,
                }));
            } else {
                let versions = dependency.versions().to_vec();
                result.0.push(ManifestsRequest::NotPinned(NotPinnedRequest {
                    id: RequestIdentifier {
                        source: *source,
                        name: dependency.name(),
                    },
                    versions: if !versions.is_empty() {
                        Some(versions)
                    } else {
                        None
                    },
                    features,
                }))
            }
        }
        Ok(result)
    }
}

/// Type representing the state of a fetch.
/// The requests in the Pending version signify which requests want to use the result of this fetch.
#[derive(Debug)]
enum QueryState {
    Failed,
    Pending { requests: Vec<ManifestsRequest> },
    Done,
}

/// Type representing what action to perform for a given request.
#[derive(Debug)]
pub enum RequestAction {
    /// A fetch for such request was never made, so the fetch should be performed.
    Fetch,
    /// No need for a fetch, but further requests result from this one.
    More { requests: Vec<ManifestsRequest> },
}

impl Default for RequestAction {
    fn default() -> Self {
        Self::More { requests: vec![] }
    }
}

/// General type representing the state of the gathering process.
/// Monitors the state of all the fetches, contains the data of all the packages and other necessary info.
///
/// Note(terminology):
/// ------------------
/// 1. a *request* signifies a need to read the manifest of a given package (pinned request) or the
///    manifests of all the packages with a given source and name satisfying some versions constraints (not pinned request),
/// 2. a *fetch* is a process of obtaining manifest(s) for the first time, for example from Ducknest,
/// 3. to satisfy a *request*, a *fetch* may be made, this usually happens for the first *request* referencing a specific source and name/package.
/// 4. requests are identified by a pair (source, name), which is called the request's id.
///
/// Pinned & Not Pinned vs Registry, Git & Local:
/// ---------------------
/// 1. Git and Local requests are always not pinned, though they can only return one manifest.
/// 2. Registry requests can be either pinned or not pinned.
#[derive(Debug, Default)]
pub struct GathererState {
    /// Tracks the state of all the pending or finished not pinned fetches.
    not_pinned_fetches: HashMap<RequestIdentifier, QueryState>,
    /// Tracks the state of all the pending or finished pinned fetches.
    pinned_fetches: HashMap<WithVersion<RequestIdentifier>, QueryState>,

    pkgs_data: HashMap<WithVersion<FullIdentity>, PackageData>,
    versions_for_identity: HashMap<FullIdentity, HashSet<Version>>,
    source_to_origin_resolver: HashMap<Source, FullOrigin>,
}

impl GathererState {
    /// Returns what action to perform for a given request.
    pub fn get_request_action(
        &mut self,
        request: ManifestsRequest,
    ) -> GathererResult<RequestAction> {
        match request {
            ManifestsRequest::Pinned(pinned_request) => {
                self.get_request_action_pinned(pinned_request)
            }
            ManifestsRequest::NotPinned(not_pinned_request) => {
                self.get_request_action_not_pinned(not_pinned_request)
            }
        }
    }

    /// Returns what action to perform for a given not pinned request.
    fn get_request_action_not_pinned(
        &mut self,
        not_pinned_request: NotPinnedRequest,
    ) -> GathererResult<RequestAction> {
        let Some(fetch_state) = self.not_pinned_fetches.get_mut(&not_pinned_request.id) else {
            self.not_pinned_fetches.insert(
                not_pinned_request.id,
                QueryState::Pending {
                    requests: vec![ManifestsRequest::NotPinned(not_pinned_request)],
                },
            );
            return Ok(GathererComputation::only_success(RequestAction::Fetch));
        };
        match fetch_state {
            QueryState::Failed => {
                // The fetch has already failed before.
                Ok(GathererComputation::empty())
            }
            QueryState::Pending { requests } => {
                requests.push(ManifestsRequest::NotPinned(not_pinned_request));
                Ok(GathererComputation::empty())
            }
            QueryState::Done => {
                let result = self.update_features_for_versions_with_selector(not_pinned_request)?;
                Ok(GathererComputation(
                    RequestAction::More { requests: result.0 },
                    result.1,
                ))
            }
        }
    }

    /// Returns what action to perform for a given pinned request.
    fn get_request_action_pinned(
        &mut self,
        pinned_request: PinnedRequest,
    ) -> GathererResult<RequestAction> {
        let request_pkg = WithVersion::new(pinned_request.id, pinned_request.version);
        let Some(fetch_state) = self.pinned_fetches.get_mut(&request_pkg) else {
            // If the pinned request has not been made, we may still have done an unpinned request for the corresponding source and name.
            let Some(not_pinned_fetch_state) = self.not_pinned_fetches.get_mut(&pinned_request.id)
            else {
                self.pinned_fetches.insert(
                    request_pkg,
                    QueryState::Pending {
                        requests: vec![ManifestsRequest::Pinned(pinned_request)],
                    },
                );
                return Ok(GathererComputation::only_success(RequestAction::Fetch));
            };
            match not_pinned_fetch_state {
                QueryState::Failed => {
                    // The not pinned fetch has failed but maybe the pinned one will be successful.
                    self.pinned_fetches.insert(
                        request_pkg,
                        QueryState::Pending {
                            requests: vec![ManifestsRequest::Pinned(pinned_request)],
                        },
                    );
                    return Ok(GathererComputation::only_success(RequestAction::Fetch));
                }
                QueryState::Pending { requests } => {
                    // We are adding a pinned request to the requests chained to an unpinned fetch.
                    // This is not a bug - later the same method `[GathererState::complete_requests]` will be used
                    // for handling chained requests for both types of fetches, so this pinned
                    // request will be properly handled when the not pinned fetch completes.
                    requests.push(ManifestsRequest::Pinned(pinned_request));
                    return Ok(GathererComputation::empty());
                }
                QueryState::Done => {
                    let answer_pkg = request_pkg
                        .resolve(&self.source_to_origin_resolver)
                        .with_context_internal(|| {
                            format!("could not expand the package {:?}", request_pkg)
                        })?;
                    if !self.pkgs_data.contains_key(&answer_pkg) {
                        qp_bail_internal!(
                            "pinned package {answer_pkg:?} was supposed to be already fetched by a not pinned fetch but has no data"
                        );
                    }
                    let result = self.update_features(answer_pkg, pinned_request.features)?;
                    return Ok(GathererComputation(
                        RequestAction::More { requests: result.0 },
                        result.1,
                    ));
                }
            }
        };
        match fetch_state {
            QueryState::Failed => {
                // The fetch has already failed before.
                Ok(GathererComputation::empty())
            }
            QueryState::Pending { requests } => {
                requests.push(ManifestsRequest::Pinned(pinned_request));
                Ok(GathererComputation::empty())
            }
            QueryState::Done => {
                let answer_pkg = request_pkg
                    .resolve(&self.source_to_origin_resolver)
                    .with_context_internal(|| {
                        format!("could not expand the package {:?}", request_pkg)
                    })?;
                let result = self.update_features(answer_pkg, pinned_request.features)?;
                Ok(GathererComputation(
                    RequestAction::More { requests: result.0 },
                    result.1,
                ))
            }
        }
    }

    /// After getting a response to a fetch,
    /// updates the state and decides what further requests to make.
    pub fn handle_fetch_response(
        &mut self,
        response: FetchResponse,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        match response {
            FetchResponse::Success(success) => self.handle_fetch_success(success),
            FetchResponse::Failed(failure) => self.handle_fetch_failure(failure),
        }
    }

    /// After getting a successful response to a fetch,
    /// updates the state and decides what further requests to make.
    fn handle_fetch_success(
        &mut self,
        successful_response: FetchSuccess,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        match successful_response {
            FetchSuccess::Pinned(pinned) => self.handle_success_pinned(pinned),
            FetchSuccess::NotPinned(not_pinned) => self.handle_success_not_pinned(not_pinned),
        }
    }

    /// Handles a successful response to a pinned fetch.
    /// Checks that the found package's version and name agree with the requested ones.
    fn handle_success_pinned(
        &mut self,
        pinned_success: PinnedSuccess,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let request_pkg = WithVersion::new(pinned_success.origin_id, pinned_success.origin_version);
        let Some(state) = self.pinned_fetches.get_mut(&request_pkg) else {
            qp_bail_internal!("response with no associated request state");
        };
        let QueryState::Pending { requests } = state else {
            qp_bail_internal!("query not in PENDING state");
        };
        let requests = requests.clone();
        *state = QueryState::Done;

        // If received response declares a different version, the request failed.
        if pinned_success.origin_version != pinned_success.answer_package.version()
            || pinned_success.origin_id.name != pinned_success.answer_package.value().name()
        {
            return Ok(self
                .fail_incoherent_success_pinned(
                    request_pkg,
                    "Fetched manifest's version differs from required",
                )?
                .context(MessageError(
                    format!(
                        "while handling response for the fetch of a package {} in version {}",
                        request_pkg.value().name,
                        request_pkg.version()
                    )
                    .into(),
                )));
        }

        self.source_to_origin_resolver.insert(
            pinned_success.origin_id.source,
            pinned_success.answer_package.value().origin(),
        );
        self.insert_manifests([(
            pinned_success.answer_package,
            pinned_success.fetched_manifest,
        )]);
        self.complete_requests(requests)
    }

    /// Creates errors for a successful pinned response incoherent with the request.
    fn fail_incoherent_success_pinned<U: Default>(
        &mut self,
        pkg: WithVersion<RequestIdentifier>,
        reason: impl Into<Cow<'static, str>>,
    ) -> GathererResult<U> {
        let Some(status) = self.pinned_fetches.get_mut(&pkg) else {
            return Err(
                qp_internal!("Failed request with no status").context(MessageError(reason.into()))
            );
        };
        *status = QueryState::Failed;
        Ok(GathererComputation::empty().context(MessageError(reason.into())))
    }

    /// Handles a successful response to a a not pinned fetch.
    /// Checks that:
    ///  * all the returned packages have common identity,
    ///  * the identitie's name agrees with the requested name.
    fn handle_success_not_pinned(
        &mut self,
        not_pinned_response: NotPinnedSuccess,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let Some(state) = self
            .not_pinned_fetches
            .get_mut(&not_pinned_response.origin_id)
        else {
            qp_bail_internal!("response with no associated request state");
        };
        let QueryState::Pending { requests } = state else {
            qp_bail_internal!("query not in PENDING state");
        };
        let requests = requests.clone();
        *state = QueryState::Done;

        let answer_identities: HashSet<FullIdentity> = not_pinned_response
            .fetched_manifests
            .keys()
            .map(|pkg| *pkg.value())
            .collect();
        if answer_identities.len() == 1
            && let Some(answer_identity) = answer_identities.into_iter().next()
            && answer_identity.name() == not_pinned_response.origin_id.name
        {
            self.source_to_origin_resolver.insert(
                not_pinned_response.origin_id.source,
                answer_identity.origin(),
            );
            self.insert_manifests(not_pinned_response.fetched_manifests);
            self.complete_requests(requests)
        } else {
            Ok(self
                .fail_incoherent_success_not_pinned(
                    not_pinned_response.origin_id,
                    "invalid fetch response",
                )?
                .context(MessageError(
                    format!(
                        "While handling response for the fetch of {:?}",
                        not_pinned_response.origin_id
                    )
                    .into(),
                )))
        }
    }

    /// Creates errors for a successful not pinned response incoherent with the request.
    fn fail_incoherent_success_not_pinned<U: Default>(
        &mut self,
        id: RequestIdentifier,
        reason: impl Into<Cow<'static, str>>,
    ) -> GathererResult<U> {
        let Some(status) = self.not_pinned_fetches.get_mut(&id) else {
            return Err(
                qp_internal!("failed request with no status").context(MessageError(reason.into()))
            );
        };
        *status = QueryState::Failed;
        Ok(GathererComputation::empty().context(MessageError(reason.into())))
    }

    /// After a failed fetch, updates the state and decides what further requests to make.
    fn handle_fetch_failure(
        &mut self,
        failure_response: FetchFailure,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        match failure_response {
            FetchFailure::Pinned(pinned_failure) => self.handle_failure_pinned(pinned_failure),
            FetchFailure::NotPinned(not_pinned_failure) => {
                self.handle_failure_not_pinned(not_pinned_failure)
            }
        }
    }

    /// Handles a failed pinned fetch.
    fn handle_failure_pinned(
        &mut self,
        failure_pinned_response: PinnedFailure,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let origin_package = WithVersion::new(
            failure_pinned_response.origin_id,
            failure_pinned_response.origin_version,
        );
        let Some(state) = self.pinned_fetches.get_mut(&origin_package) else {
            qp_bail_internal!("response with no associated request state");
        };
        *state = QueryState::Failed;
        Ok(GathererComputation::empty())
    }

    /// Handles a failed not pinned request.
    fn handle_failure_not_pinned(
        &mut self,
        failure_not_pinned_response: NotPinnedFailure,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let Some(state) = self
            .not_pinned_fetches
            .get_mut(&failure_not_pinned_response.origin_id)
        else {
            qp_bail_internal!("response with no associated request state");
        };
        *state = QueryState::Failed;
        Ok(GathererComputation::empty())
    }

    /// Inserts manifests gotten in a fetch response into the GathererState.
    fn insert_manifests(
        &mut self,
        manifests: impl IntoIterator<Item = (WithVersion<FullIdentity>, Box<Manifest>)>,
    ) {
        for (pkg, manifest) in manifests.into_iter() {
            self.versions_for_identity
                .entry(*pkg.value())
                .or_default()
                .insert(pkg.version());
            self.pkgs_data.entry(pkg).or_insert_with(|| PackageData {
                manifest,
                requested_features: HashSet::new(),
                referenced_by_requests: false,
            });
        }
    }

    /// For each request referencing a given fetch, updates requested features of the fetched packages,
    /// returning resulting new requests.
    fn complete_requests(
        &mut self,
        requests: Vec<ManifestsRequest>,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let mut result = GathererComputation::empty();
        for request in requests {
            match request {
                ManifestsRequest::Pinned(pinned_request) => {
                    let Some(answer_origin) = self
                        .source_to_origin_resolver
                        .get(&pinned_request.id.source)
                    else {
                        qp_bail_internal!("could not resolve source {:?}", pinned_request.id.source)
                    };
                    let answer_identity = FullIdentity::new(pinned_request.id.name, *answer_origin);
                    let Some(versions) = self.versions_for_identity.get(&answer_identity) else {
                        qp_bail_internal!("no gathered versions for identity {answer_identity:?}")
                    };
                    if !versions.contains(&pinned_request.version) {
                        result.1.push(qp_err!(
                            "request of pinned dependency {} could not find matching version {}",
                            pinned_request.id.name,
                            pinned_request.version,
                        ));
                        return Ok(result);
                    }
                    result.extend(self.update_features(
                        WithVersion::new(answer_identity, pinned_request.version),
                        pinned_request.features,
                    )?);
                }
                ManifestsRequest::NotPinned(not_pinned_request) => {
                    result.extend(
                        self.update_features_for_versions_with_selector(not_pinned_request)?,
                    );
                }
            }
        }
        Ok(result)
    }

    /// After an unpinned fetch, considers all gotten packages.
    /// Selects versions satisfying a given selector and updates features for them.
    fn update_features_for_versions_with_selector(
        &mut self,
        request: NotPinnedRequest,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let id = request.id;
        let selector = request.versions;
        let requested_features = request.features;
        let mut result: GathererComputation<Vec<ManifestsRequest>> = GathererComputation::empty();
        let mut any_matched = false;
        let Some(answer_origin) = self.source_to_origin_resolver.get(&id.source).copied() else {
            qp_bail_internal!("could not resolve source {:?}", id.source);
        };
        let answer_identity = FullIdentity::new(id.name, answer_origin);
        let versions: Vec<Version> = self
            .versions_for_identity
            .get(&answer_identity)
            .iter()
            .copied()
            .flatten()
            .copied()
            .collect();
        for version in versions {
            if selector.is_none()
                || selector
                    .iter()
                    .flatten()
                    .any(|selector| selector.can_be_upgraded_to(&version))
            {
                any_matched = true;
                result.extend(self.update_features(
                    WithVersion::new(answer_identity, version),
                    requested_features.clone(),
                )?);
            }
        }
        let selector_text = selector.iter().flatten().join(", ");
        if !any_matched {
            result.1.push(
                QuackError::message(
            format!(
                "there is a dependency on package of name {} with versions {selector_text}, but no matching versions exist",
                id.name,
            )));
        }
        Ok(result)
    }

    /// Updates what new features of a package were potentially requested.
    /// If any new feature has been added, returns requests for package's dependencies.
    fn update_features(
        &mut self,
        pkg: WithVersion<FullIdentity>,
        mut requested_features: HashSet<FeatureName>,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let Some(pkg_data) = self.pkgs_data.get_mut(&pkg) else {
            qp_bail_internal!("Fetched package {pkg:?} without PackageData")
        };
        let mut nonexistent_features = vec![];
        for feature in requested_features.iter() {
            if !pkg_data.manifest.features().has_feature(*feature) {
                nonexistent_features.push(*feature);
            }
        }
        for feature in nonexistent_features.iter() {
            requested_features.remove(feature);
        }
        if !nonexistent_features.is_empty() {
            let package = pkg.value().descriptive_name();
            let missing_features = nonexistent_features.join(", ");
            let plural = if nonexistent_features.len() == 1 {
                ""
            } else {
                "s"
            };
            return Ok(GathererComputation::only_error(qp_err!(
                "package {package} does not have feature{plural} `{missing_features}`"
            )));
        }
        let manifest = &pkg_data.manifest;
        let requested_features = manifest.features().expand_features(requested_features)?;
        if pkg_data
            .requested_features
            .extend_and_get_diff_size(requested_features)
            > 0
            || !pkg_data.referenced_by_requests
        {
            pkg_data.referenced_by_requests = true;
            pkg_data.dep_requests()
        } else {
            Ok(GathererComputation::empty())
        }
    }
}

/// A struct containing all the information gathered by the gatherer.
#[derive(Debug)]
pub struct GatheredInfo {
    /// The gathered manifests of the packages referenced in requests.
    pub gathered_manifests: HashMap<WithVersion<FullIdentity>, Box<Manifest>>,
    /// The intersection of the manifest defined features and features referenced in the requests.
    pub possible_features: HashMap<WithVersion<FullIdentity>, HashSet<FeatureName>>,
    /// The set of the possible versions of the packages with a given identity.
    pub versions_for_identity: HashMap<FullIdentity, HashSet<Version>>,
    /// The translation from [`Source`] to [`FullIdentity`].
    pub source_to_origin_resolver: HashMap<Source, FullOrigin>,
}

impl TryFrom<GathererState> for GatheredInfo {
    type Error = QuackError;

    fn try_from(mut value: GathererState) -> QuackResult<Self> {
        let mut gathered_manifests = HashMap::new();
        let mut possible_features = HashMap::new();
        let mut unnecessary_pkgs = Vec::new();
        for (pkg, data) in value.pkgs_data {
            if data.referenced_by_requests {
                gathered_manifests.insert(pkg, data.manifest);
                possible_features.insert(pkg, data.requested_features);
            } else {
                unnecessary_pkgs.push(pkg);
            }
        }
        for pkg in unnecessary_pkgs {
            let Some(versions) = value.versions_for_identity.get_mut(pkg.value()) else {
                qp_bail_internal!(
                    "unnecessary package's identity not present in the versions for identity map"
                );
            };
            versions.remove(&pkg.version());
        }
        Ok(GatheredInfo {
            gathered_manifests,
            possible_features,
            versions_for_identity: value.versions_for_identity,
            source_to_origin_resolver: value.source_to_origin_resolver,
        })
    }
}
