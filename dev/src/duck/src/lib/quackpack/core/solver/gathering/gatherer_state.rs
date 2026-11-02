// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::gathering::fetch_types::{
    FetchFailure, FetchResponse, FetchSuccess, ManifestsRequest, NotPinnedFailure,
    NotPinnedRequest, NotPinnedSuccess, PinnedFailure, PinnedRequest, PinnedSuccess, RequestAction,
    RequestIdentifier,
};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{FeatureName, Manifest, PackageId, Selector, Source, Version};
use crate::quackpack::util::str_id::QpJoin;
use crate::quackpack::util::with_version::WithVersion;
use crate::util::Pluralize;
use crate::util::error::ErrorsLogger;
use crate::util::extend::QpExtend;
use crate::{QuackError, QuackResult, QuackResultContext, qp_bail_internal, qp_err};

/// Gathered information about a particular package.
#[derive(Debug)]
pub struct PackageData {
    /// Fetched manifest of the package.
    pub manifest: Box<Manifest>,
    /// The intersection of the manifest defined features and features referenced in the requests.
    pub requested_features: HashSet<FeatureName>,
    /// Whether any manifest request referenced this package or not.
    /// This being false can happen due to not pinned registry requests.
    /// For such a request we fetch all manifests, not only those with compatible versions.
    pub referenced_by_requests: bool,
}

impl PackageData {
    /// Returns a list of requests for possible realizations of package's dependencies,
    /// given already requested features of the package.
    fn dep_requests(&self) -> QuackResult<Vec<ManifestsRequest>> {
        if !self.referenced_by_requests {
            return Ok(vec![]);
        }
        let mut result = vec![];
        for dependency in self
            .manifest
            .dependencies()
            .select(&Selector::EnabledBy(&self.requested_features))
        {
            let source = dependency.source();
            let features: HashSet<FeatureName> =
                HashSet::from_iter(dependency.enabled_features(&self.requested_features));
            if dependency.is_pinned() {
                let version = dependency
                    .versions()
                    .first()
                    .copied()
                    .with_context_internal(|| {
                        format!("pinned dependency without version: {dependency:#?}")
                    })?;
                result.push(ManifestsRequest::new_pinned(
                    source,
                    dependency.name(),
                    version,
                    features,
                ));
            } else {
                let versions = dependency.versions().to_vec();
                let versions = if !versions.is_empty() {
                    Some(versions)
                } else {
                    None
                };
                result.push(ManifestsRequest::new_not_pinned(
                    source,
                    dependency.name(),
                    versions,
                    features,
                ));
            }
        }
        Ok(result)
    }
}

/// Type representing the state of a fetch.
/// The requests in the Pending version signify which requests want to use the result of this fetch.
#[derive(Debug, Clone)]
enum QueryState {
    Failed,
    Pending { requests: Vec<ManifestsRequest> },
    Done,
}

impl QueryState {
    /// Creates a one [`QueryState::Pending`] with one pinned request.
    fn pending_pinned(request: PinnedRequest) -> Self {
        Self::Pending {
            requests: vec![ManifestsRequest::Pinned(request)],
        }
    }

    /// Creates a one [`QueryState::Pending`] with one not pinned request.
    fn pending_not_pinned(request: NotPinnedRequest) -> Self {
        Self::Pending {
            requests: vec![ManifestsRequest::NotPinned(request)],
        }
    }
}

/// General type representing the state of the gathering process.
/// Monitors the state of all the fetches, contains the data of all the packages and other necessary info.
#[derive(Debug, Default)]
pub struct GathererState {
    /// Tracks the state of all the pending or finished not pinned fetches.
    not_pinned_fetches: HashMap<RequestIdentifier, QueryState>,
    /// Tracks the state of all the pending or finished pinned fetches.
    pinned_fetches: HashMap<WithVersion<RequestIdentifier>, QueryState>,

    /// [`PackageData`] for all the gathered packages.
    pkgs_data: HashMap<PackageId, PackageData>,
    /// Set of versions of packages found for a given [`FullIdentity`].
    versions_for_identity: HashMap<FullIdentity, HashSet<Version>>,
    /// A dictionary of translations from [`Source`] to [`FullOrigin`].
    source_to_origin_resolver: HashMap<Source, FullOrigin>,
}

// Methods of `GathererState` related to deciding, given a `ManifestsReques`t, what action to perform.
// This is either a fetch or more `ManifestsRequests` to consider.
impl GathererState {
    /// Returns what action to perform for a given request.
    pub fn get_request_action(
        &mut self,
        request: ManifestsRequest,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<RequestAction> {
        match request {
            ManifestsRequest::Pinned(pinned_request) => {
                self.get_request_action_pinned(pinned_request, errors)
            }
            ManifestsRequest::NotPinned(not_pinned_request) => {
                self.get_request_action_not_pinned(not_pinned_request, errors)
            }
        }
    }

    /// Returns what action to perform for a given not pinned request.
    fn get_request_action_not_pinned(
        &mut self,
        not_pinned_request: NotPinnedRequest,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<RequestAction> {
        let Some(fetch_state) = self.not_pinned_fetches.get_mut(&not_pinned_request.id) else {
            self.not_pinned_fetches.insert(
                not_pinned_request.id,
                QueryState::pending_not_pinned(not_pinned_request),
            );
            return Ok(RequestAction::Fetch);
        };
        match fetch_state {
            QueryState::Failed => {
                // The fetch has already failed before.
                Ok(RequestAction::default())
            }
            QueryState::Pending { requests } => {
                requests.push(ManifestsRequest::NotPinned(not_pinned_request));
                Ok(RequestAction::default())
            }
            QueryState::Done => Ok(self
                .update_pkg_data_for_not_pinned(not_pinned_request, errors)?
                .into()),
        }
    }

    /// Returns what action to perform for a given pinned request.
    fn get_request_action_pinned(
        &mut self,
        pinned_request: PinnedRequest,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<RequestAction> {
        let request_pkg = WithVersion::new(pinned_request.id, pinned_request.version);
        let Some(fetch_state) = self.pinned_fetches.get_mut(&request_pkg) else {
            // This request comes for the first time.
            return self.get_request_action_fresh_pinned(request_pkg, pinned_request, errors);
        };
        match fetch_state {
            QueryState::Failed => {
                // The fetch has already failed before.
                Ok(RequestAction::default())
            }
            QueryState::Pending { requests } => {
                requests.push(ManifestsRequest::Pinned(pinned_request));
                Ok(RequestAction::default())
            }
            QueryState::Done => {
                let answer_pkg = request_pkg
                    .resolve(&self.source_to_origin_resolver)
                    .with_context_internal(|| {
                        format!("could not expand the package {request_pkg:#?} {self:#?}")
                    })?;
                Ok(self
                    .update_pkg_data(answer_pkg, pinned_request.features, errors)?
                    .into())
            }
        }
    }

    /// Returns what action to perform for a given pinned request, if it is the first time this request comes.
    /// There was no pinned request but there might be a not pinned request for the same [`Source`].
    fn get_request_action_fresh_pinned(
        &mut self,
        request_pkg: WithVersion<RequestIdentifier>,
        pinned_request: PinnedRequest,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<RequestAction> {
        let Some(not_pinned_fetch_state) = self.not_pinned_fetches.get_mut(&pinned_request.id)
        else {
            // There was no not pinned request either, so just request a fetch.
            self.pinned_fetches
                .insert(request_pkg, QueryState::pending_pinned(pinned_request));
            return Ok(RequestAction::Fetch);
        };
        match not_pinned_fetch_state {
            QueryState::Failed => {
                // The not pinned fetch has failed but maybe the pinned one will be successful.
                self.pinned_fetches
                    .insert(request_pkg, QueryState::pending_pinned(pinned_request));
                Ok(RequestAction::Fetch)
            }
            QueryState::Pending { requests } => {
                // We are adding a pinned request to the requests chained to an unpinned fetch.
                // This is not a bug - later the same method [`GathererState::complete_requests`] will be used
                // for handling chained requests for both types of fetches, so this pinned
                // request will be properly handled when the not pinned fetch completes.
                requests.push(ManifestsRequest::Pinned(pinned_request));
                Ok(RequestAction::default())
            }
            QueryState::Done => {
                let answer_pkg = request_pkg
                    .resolve(&self.source_to_origin_resolver)
                    .with_context_internal(|| {
                        format!("could not expand the package {request_pkg:#?} {self:#?}")
                    })?;
                if !self.pkgs_data.contains_key(&answer_pkg) {
                    qp_bail_internal!(
                        "pinned package {answer_pkg:?} was supposed to be already fetched by a not pinned fetch but has no data {self:#?}"
                    );
                }
                Ok(self
                    .update_pkg_data(answer_pkg, pinned_request.features, errors)?
                    .into())
            }
        }
    }

    /// Given a not pinned request, selects versions satisfying the selector and updates pkg_data for them.
    /// Checks that at least one version satisfied the selector, if not this is reported as an error.
    fn update_pkg_data_for_not_pinned(
        &mut self,
        request: NotPinnedRequest,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<Vec<ManifestsRequest>> {
        let id = request.id;
        let selector = request.versions;
        let requested_features = request.features;
        let mut result = vec![];
        let mut any_matched = false;
        let Some(answer_origin) = self.source_to_origin_resolver.get(&id.source).copied() else {
            qp_bail_internal!("could not resolve source {:?} {self:#?}", id.source);
        };
        let answer_identity = FullIdentity::new(id.name, answer_origin);
        let versions: Vec<Version> = self
            .versions_for_identity
            .get(&answer_identity)
            .into_iter()
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
                result.extend(self.update_pkg_data(
                    PackageId::new(answer_identity, version),
                    requested_features.clone(),
                    errors,
                )?);
            }
        }
        if !any_matched {
            let selector_text = selector.iter().flatten().join(", ");
            errors.log(
            qp_err!(
                "there is a dependency on package of name {} with versions {selector_text}, but no matching versions exist",
                id.name,
            ));
            return Ok(vec![]);
        }
        Ok(result)
    }

    /// Updates the [`PackageData`] of a package, especially what new features of the package were requested.
    /// Returns a vector of new [`ManifestsRequest`]s this update entails.
    ///  
    /// If any new feature has been added, this is the list of requests for package's dependencies,
    /// otherwise an empty vector.
    /// This is because the new features might have just enabled some dependency or forced new features of some dependency,
    /// so we consider all of the dependencies ones again.
    ///
    /// Checks that all the requested features exist (are supported by the package).
    /// If any such feature exists, then the [`ManifestsRequest`]
    fn update_pkg_data(
        &mut self,
        pkg: PackageId,
        requested_features: HashSet<FeatureName>,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<Vec<ManifestsRequest>> {
        let Some(pkg_data) = self.pkgs_data.get_mut(&pkg) else {
            qp_bail_internal!("fetched package {pkg:?} without PackageData {self:#?}")
        };
        // Collect features which were requested but the package does not have them.
        let mut nonexistent_features = vec![];
        for feature in requested_features.iter() {
            if !pkg_data.manifest.features().has_feature(*feature) {
                nonexistent_features.push(*feature);
            }
        }
        if !nonexistent_features.is_empty() {
            let package = pkg.identity().descriptive_name();
            let missing_features = nonexistent_features.join(", ");
            errors.log(qp_err!(
                "package {package} does not have feature{} `{missing_features}`",
                nonexistent_features.s_if_plural(),
            ));
            return Ok(vec![]);
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
            Ok(vec![])
        }
    }
}

// Methods of `GathererState` related to handling fetch results.
impl GathererState {
    /// After getting a response to a fetch,
    /// updates the state and decides what further requests to make.
    ///
    /// Note that only manifests requests for the dependencies of the packages from the response can be made.
    /// Indeed, those packages are already fetched, so
    pub fn handle_fetch_response(
        &mut self,
        response: FetchResponse,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<Vec<ManifestsRequest>> {
        match response {
            FetchResponse::Success(success) => self.handle_fetch_success(success, errors),
            FetchResponse::Failed(failure) => self.handle_fetch_failure(failure),
        }
    }

    /// After getting a successful response to a fetch,
    /// updates the state and decides what further requests to make.
    fn handle_fetch_success(
        &mut self,
        successful_response: FetchSuccess,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<Vec<ManifestsRequest>> {
        match successful_response {
            FetchSuccess::Pinned(pinned) => self.handle_success_pinned(pinned, errors),
            FetchSuccess::NotPinned(not_pinned) => {
                self.handle_success_not_pinned(not_pinned, errors)
            }
        }
    }

    /// Handles a successful response to a pinned fetch.
    fn handle_success_pinned(
        &mut self,
        pinned_success: PinnedSuccess,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<Vec<ManifestsRequest>> {
        let request_pkg = WithVersion::new(pinned_success.origin_id, pinned_success.origin_version);
        let Some(state) = self.pinned_fetches.get_mut(&request_pkg) else {
            qp_bail_internal!(
                "response `{request_pkg:?}` with no associated request state {self:#?}"
            );
        };
        let QueryState::Pending { requests } = state else {
            // HACK: Otherwise we get `cannot borrow self as immutable`, because state is mutable.
            // However, other two states are trivially copyable.
            let state = state.clone();
            qp_bail_internal!("query not in PENDING state {state:?}, {request_pkg:?}, {self:#?}");
        };
        let requests = requests.clone();
        *state = QueryState::Done;

        self.source_to_origin_resolver.insert(
            pinned_success.origin_id.source,
            pinned_success.answer_package.origin(),
        );
        self.insert_manifests([(
            pinned_success.answer_package,
            pinned_success.fetched_manifest,
        )]);
        self.complete_requests(requests, errors)
    }

    /// Handles a successful response to a a not pinned fetch.
    fn handle_success_not_pinned(
        &mut self,
        not_pinned_response: NotPinnedSuccess,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<Vec<ManifestsRequest>> {
        let Some(state) = self
            .not_pinned_fetches
            .get_mut(&not_pinned_response.origin_id)
        else {
            qp_bail_internal!(
                "response {not_pinned_response:?} with no associated request state {self:#?}"
            );
        };
        let QueryState::Pending { requests } = state else {
            // HACK: Otherwise we get `cannot borrow self as immutable`, because state is mutable.
            // However, other two states are trivially copyable.
            let state = state.clone();
            qp_bail_internal!(
                "query not in PENDING state: `{state:?}` `{not_pinned_response:?}` {self:#?}"
            );
        };
        let requests = requests.clone();
        *state = QueryState::Done;

        let Some(answer_identity) = not_pinned_response
            .fetched_manifests
            .keys()
            .next()
            .map(|pkg| pkg.identity())
        else {
            qp_bail_internal!(
                "successful not pinned fetch result despite no manifests: {not_pinned_response:?}"
            );
        };
        self.source_to_origin_resolver.insert(
            not_pinned_response.origin_id.source,
            answer_identity.origin(),
        );
        self.insert_manifests(not_pinned_response.fetched_manifests);
        self.complete_requests(requests, errors)
    }

    /// After a failed fetch, updates the state and decides what further requests to make.
    fn handle_fetch_failure(
        &mut self,
        failure_response: FetchFailure,
    ) -> QuackResult<Vec<ManifestsRequest>> {
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
    ) -> QuackResult<Vec<ManifestsRequest>> {
        let origin_package = WithVersion::new(
            failure_pinned_response.origin_id,
            failure_pinned_response.origin_version,
        );
        let Some(state) = self.pinned_fetches.get_mut(&origin_package) else {
            qp_bail_internal!(
                "response with no associated request state `{origin_package:?}` {self:#?}"
            );
        };
        *state = QueryState::Failed;
        Ok(vec![])
    }

    /// Handles a failed not pinned request.
    fn handle_failure_not_pinned(
        &mut self,
        failure_not_pinned_response: NotPinnedFailure,
    ) -> QuackResult<Vec<ManifestsRequest>> {
        let Some(state) = self
            .not_pinned_fetches
            .get_mut(&failure_not_pinned_response.origin_id)
        else {
            qp_bail_internal!(
                "response with no associated request state `{failure_not_pinned_response:?}` {self:#?}"
            );
        };
        *state = QueryState::Failed;
        Ok(vec![])
    }

    /// Inserts manifests, gotten in a fetch response, into the GathererState.
    fn insert_manifests(
        &mut self,
        manifests: impl IntoIterator<Item = (PackageId, Box<Manifest>)>,
    ) {
        for (pkg, manifest) in manifests.into_iter() {
            self.versions_for_identity
                .entry(pkg.identity())
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
    ///
    /// Logs an error for pinned request which did not find a matching version.
    /// This might happen if the pinned request was made when a not pinned request was pending.
    fn complete_requests(
        &mut self,
        requests: Vec<ManifestsRequest>,
        errors: &mut ErrorsLogger,
    ) -> QuackResult<Vec<ManifestsRequest>> {
        let mut result = vec![];
        for request in requests {
            match request {
                ManifestsRequest::Pinned(pinned_request) => {
                    let Some(answer_origin) = self
                        .source_to_origin_resolver
                        .get(&pinned_request.id.source)
                    else {
                        qp_bail_internal!("could not resolve source `{pinned_request:?}` {self:#?}")
                    };
                    let answer_identity = FullIdentity::new(pinned_request.id.name, *answer_origin);
                    let Some(versions) = self.versions_for_identity.get(&answer_identity) else {
                        qp_bail_internal!(
                            "no gathered versions for identity {answer_identity:?} {self:#?}"
                        )
                    };
                    if !versions.contains(&pinned_request.version) {
                        errors.log(qp_err!(
                            "request of pinned dependency {} could not find matching version {}",
                            pinned_request.id.name,
                            pinned_request.version,
                        ));
                        return Ok(vec![]);
                    }
                    result.extend(self.update_pkg_data(
                        PackageId::new(answer_identity, pinned_request.version),
                        pinned_request.features,
                        errors,
                    )?);
                }
                ManifestsRequest::NotPinned(not_pinned_request) => {
                    result.extend(self.update_pkg_data_for_not_pinned(not_pinned_request, errors)?);
                }
            }
        }
        Ok(result)
    }
}

/// A struct containing all the information gathered by the gatherer.
#[derive(Debug)]
pub struct GatheredInfo {
    /// Mapping from packages to gathered data about each package.
    pub packages_data: HashMap<PackageId, PackageData>,
    /// The set of the possible versions of the packages with a given identity.
    pub versions_for_identity: HashMap<FullIdentity, HashSet<Version>>,
    /// The translation from [`Source`] to [`FullIdentity`].
    pub source_to_origin_resolver: HashMap<Source, FullOrigin>,
}

impl TryFrom<GathererState> for GatheredInfo {
    type Error = QuackError;

    fn try_from(mut value: GathererState) -> QuackResult<Self> {
        let mut unnecessary_pkgs = Vec::new();
        value.pkgs_data.retain(|pkg, data| {
            if !data.referenced_by_requests {
                unnecessary_pkgs.push(*pkg);
                return false;
            }
            true
        });
        for pkg in unnecessary_pkgs {
            let Some(versions) = value.versions_for_identity.get_mut(&pkg.identity()) else {
                qp_bail_internal!(
                    "unnecessary package's identity not present in the versions for identity map {pkg:?} {:#?}",
                    value.versions_for_identity
                );
            };
            versions.remove(&pkg.version());
        }
        Ok(GatheredInfo {
            packages_data: value.pkgs_data,
            versions_for_identity: value.versions_for_identity,
            source_to_origin_resolver: value.source_to_origin_resolver,
        })
    }
}
