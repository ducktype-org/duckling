use std::borrow::Cow;
use std::collections::{HashMap, HashSet};

use crate::quackpack::core::solver::gathering::error_suppression::{
    GathererComputation, GathererResult,
};
use crate::quackpack::core::solver::gathering::fetch_types::{
    FetchFailure, FetchResponse, FetchSuccess, ManifestsRequest, NotPinnedFailure,
    NotPinnedRequest, NotPinnedSuccess, PinnedFailure, PinnedRequest, PinnedSuccess,
};
use crate::quackpack::core::solver::types_common::{
    ExpandedLocation, ExpandedPackage, InternedLocation, Location, Package,
};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{FeatureName, Manifest, Version};
use crate::quackpack::util::str_id::QpJoin;
use crate::util::error::MessageError;
use crate::util::extend::QpExtend;
use crate::{
    QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err, qp_internal,
};

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
            let location = Location::from(dependency);
            let location = InternedLocation::new(location);
            let features: HashSet<FeatureName> = HashSet::from_iter(
                dependency.enabled_features(self.requested_features.iter().copied()),
            );
            if dependency.is_pinned() {
                let version = dependency
                    .versions()
                    .first()
                    .copied()
                    .context_internal("Pinned dependency without version")?;
                result.0.push(ManifestsRequest::Pinned(PinnedRequest {
                    location,
                    version,
                    features,
                }));
            } else {
                let versions = dependency.versions().to_vec();
                result.0.push(ManifestsRequest::NotPinned(NotPinnedRequest {
                    location,
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
///    manifests of all the packages from a given location, satisfying some versions constraints (not pinned request),
/// 2. a *fetch* is a process of obtaining manifest(s) for the first time, for example from Ducknest,
/// 3. to satisfy a *request*, a *fetch* may be made, this usually happens for the first *request* referencing a specific location/package.
///
/// Pinned & Not Pinned vs Registry, Git & Local:
/// ---------------------
/// 1. Git and Local requests are always not pinned, though they can only return one manifest.
/// 2. Registry requests can be either pinned or not pinned.
#[derive(Debug, Default)]
pub struct GathererState {
    /// Tracks the state of all the pending or finished not pinned fetches.
    not_pinned_fetches: HashMap<InternedLocation, QueryState>,
    /// Tracks the state of all the pending or finished pinned fetches.
    pinned_fetches: HashMap<Package, QueryState>,

    pkgs_data: HashMap<ExpandedPackage, PackageData>,
    versions_for_location: HashMap<ExpandedLocation, HashSet<Option<Version>>>,
    location_resolver: HashMap<InternedLocation, ExpandedLocation>,
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
        let Some(fetch_state) = self
            .not_pinned_fetches
            .get_mut(&not_pinned_request.location)
        else {
            self.not_pinned_fetches.insert(
                not_pinned_request.location,
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
                let result = self.update_features_not_pinned(
                    not_pinned_request.location,
                    not_pinned_request.versions,
                    not_pinned_request.features,
                )?;
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
        let request_pkg = Package {
            location: pinned_request.location,
            version: Some(pinned_request.version),
        };
        let Some(fetch_state) = self.pinned_fetches.get_mut(&request_pkg) else {
            // If the pinned request has not been made, we may still have done an unpinned request for the corresponding location.
            let Some(not_pinned_fetch_state) =
                self.not_pinned_fetches.get_mut(&pinned_request.location)
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
                    let expanded_package = request_pkg
                        .resolve(&self.location_resolver)
                        .with_context_internal(|| {
                            format!("Could not expand the package {:?}", request_pkg)
                        })?;
                    if !self.pkgs_data.contains_key(&expanded_package) {
                        return Ok(GathererComputation::only_error(qp_err!(
                            "Pinned package {expanded_package:?} was supposed to be already fetched by a not pinned fetch but has no data"
                        )));
                    }
                    let result = self.update_features(expanded_package, pinned_request.features)?;
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
                let expanded_package = request_pkg
                    .resolve(&self.location_resolver)
                    .with_context_internal(|| {
                        format!("Could not expand the package {:?}", request_pkg)
                    })?;
                let result = self.update_features(expanded_package, pinned_request.features)?;
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
    fn handle_success_pinned(
        &mut self,
        pinned_success: PinnedSuccess,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let origin_package = Package {
            location: pinned_success.origin_location,
            version: Some(pinned_success.origin_version),
        };
        let Some(state) = self.pinned_fetches.get_mut(&origin_package) else {
            qp_bail_internal!("Response with no associated request state");
        };
        let QueryState::Pending { requests } = state else {
            qp_bail_internal!("Query not in PENDING state");
        };
        let requests = requests.clone();
        *state = QueryState::Done;

        // If received response declares a different version, the request failed.
        if Some(pinned_success.origin_version) != pinned_success.expanded_package.version {
            return Ok(self
                .fail_incoherent_success_pinned(
                    origin_package,
                    "Fetched manifest's version differs from required",
                )?
                .context(MessageError(
                    format!(
                        "While handling response for the fetch of {:?}",
                        origin_package
                    )
                    .into(),
                )));
        }

        self.location_resolver.insert(
            pinned_success.origin_location,
            pinned_success.expanded_package.location,
        );
        self.insert_manifests([(
            pinned_success.expanded_package,
            pinned_success.fetched_manifest,
        )]);
        self.complete_requests(requests)
    }

    /// Creates errors for a successful pinned response incoherent with the request.
    fn fail_incoherent_success_pinned<U: Default>(
        &mut self,
        pkg: Package,
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
    fn handle_success_not_pinned(
        &mut self,
        not_pinned_response: NotPinnedSuccess,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let Some(state) = self
            .not_pinned_fetches
            .get_mut(&not_pinned_response.origin_location)
        else {
            qp_bail_internal!("Response with no associated request state");
        };
        let QueryState::Pending { requests } = state else {
            qp_bail_internal!("Query not in PENDING state");
        };
        let requests = requests.clone();
        *state = QueryState::Done;

        let expanded_locs: HashSet<ExpandedLocation> = not_pinned_response
            .fetched_manifests
            .keys()
            .map(|pkg| pkg.location)
            .collect();
        if expanded_locs.len() == 1
            && let Some(expanded_loc) = expanded_locs.into_iter().next()
        {
            self.location_resolver
                .insert(not_pinned_response.origin_location, expanded_loc);
            self.insert_manifests(not_pinned_response.fetched_manifests);
            self.complete_requests(requests)
        } else {
            Ok(self
                .fail_incoherent_success_not_pinned(
                    not_pinned_response.origin_location,
                    "Invalid fetch response",
                )?
                .context(MessageError(
                    format!(
                        "While handling response for the fetch of {:?}",
                        not_pinned_response.origin_location
                    )
                    .into(),
                )))
        }
    }

    /// Creates errors for a successful not pinned response incoherent with the request.
    fn fail_incoherent_success_not_pinned<U: Default>(
        &mut self,
        location: InternedLocation,
        reason: impl Into<Cow<'static, str>>,
    ) -> GathererResult<U>
where {
        let Some(status) = self.not_pinned_fetches.get_mut(&location) else {
            return Err(
                qp_internal!("Failed request with no status").context(MessageError(reason.into()))
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
        let origin_package = Package {
            location: failure_pinned_response.origin_location,
            version: Some(failure_pinned_response.origin_version),
        };
        let Some(state) = self.pinned_fetches.get_mut(&origin_package) else {
            qp_bail_internal!("Response with no associated request state");
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
            .get_mut(&failure_not_pinned_response.origin_location)
        else {
            qp_bail_internal!("Response with no associated request state");
        };
        *state = QueryState::Failed;
        Ok(GathererComputation::empty())
    }

    /// Inserts manifests gotten in a fetch response into the GathererState.
    fn insert_manifests(
        &mut self,
        manifests: impl IntoIterator<Item = (ExpandedPackage, Box<Manifest>)>,
    ) {
        for (pkg, manifest) in manifests.into_iter() {
            self.versions_for_location
                .entry(pkg.location)
                .or_default()
                .insert(pkg.version);
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
                    let Some(expanded_loc) = self.location_resolver.get(&pinned_request.location)
                    else {
                        qp_bail_internal!(
                            "Could not resolve location {:?}",
                            pinned_request.location
                        )
                    };
                    let Some(versions) = self.versions_for_location.get(expanded_loc) else {
                        qp_bail_internal!("No gathered versions for location {expanded_loc:?}")
                    };
                    if !versions.contains(&Some(pinned_request.version)) {
                        qp_bail!(
                            "Request of pinned dependency {:?} could not find matching version",
                            pinned_request.location
                        )
                    }
                    result.extend(self.update_features(
                        ExpandedPackage {
                            location: *expanded_loc,
                            version: Some(pinned_request.version),
                        },
                        pinned_request.features,
                    )?);
                }
                ManifestsRequest::NotPinned(not_pinned_request) => {
                    let location = not_pinned_request.location;
                    let selector = not_pinned_request.versions;
                    let requested_features = not_pinned_request.features;
                    result.extend(self.update_features_not_pinned(
                        location,
                        selector,
                        requested_features,
                    )?);
                }
            }
        }
        Ok(result)
    }

    /// After a fetch, considers all its associated requests, updates their associated packages data
    /// and returns necessary new requests.
    fn update_features_not_pinned(
        &mut self,
        location: InternedLocation,
        selector: Option<Vec<Version>>,
        requested_features: HashSet<FeatureName>,
    ) -> GathererResult<Vec<ManifestsRequest>> {
        let mut result: GathererComputation<Vec<ManifestsRequest>> = GathererComputation::empty();
        let mut any_matched = false;
        let Some(expanded_loc) = self.location_resolver.get(&location).copied() else {
            qp_bail_internal!("Could not resolve location {location:?}");
        };
        let versions: Vec<Option<Version>> = self
            .versions_for_location
            .get(&expanded_loc)
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
                    .any(|selector| Some(*selector).can_be_upgraded_to(&version))
            {
                any_matched = true;
                result.extend(self.update_features(
                    ExpandedPackage {
                        location: expanded_loc,
                        version,
                    },
                    requested_features.clone(),
                )?);
            }
        }
        if !any_matched {
            qp_bail!(
                "There is a dependency on package in location {location:?} with versions {selector:?}, but no matching versions exist"
            )
        }
        Ok(result)
    }

    /// Updates what new features of a package were potentially requested.
    /// If any new feature has been added, returns requests for package's dependencies.
    fn update_features(
        &mut self,
        pkg: ExpandedPackage,
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
            let package = pkg.location().descriptive_name();
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
    pub gathered_manifests: HashMap<ExpandedPackage, Box<Manifest>>,
    /// The intersection of the manifest defined features and features referenced in the requests.
    pub possible_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
    /// The set of the possible versions of the packages satisfying a given location.
    pub versions_for_location: HashMap<ExpandedLocation, HashSet<Option<Version>>>,
    /// The translation from [`InternedLocation`] to [`ExpandedLocation`].
    pub location_resolver: HashMap<InternedLocation, ExpandedLocation>,
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
            let Some(versions) = value.versions_for_location.get_mut(&pkg.location) else {
                qp_bail_internal!(
                    "Unnecessary package's location not present in the versions for location map"
                );
            };
            versions.remove(&pkg.version);
        }
        Ok(GatheredInfo {
            gathered_manifests,
            possible_features,
            versions_for_location: value.versions_for_location,
            location_resolver: value.location_resolver,
        })
    }
}
