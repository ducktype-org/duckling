use std::collections::{HashMap, HashSet};

use crate::quackpack::core::{FeatureName, Manifest, PackageId, Source, Version};
use crate::{QuackResult, StrId, qp_bail_internal};

/// Type representing a request to get manifests for a single/multiple packages.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ManifestsRequest {
    Pinned(PinnedRequest),
    NotPinned(NotPinnedRequest),
}

/// Request to get manifests for all packages from a given source, satisfying given versions selector.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NotPinnedRequest {
    pub id: RequestIdentifier,
    pub versions: Option<Vec<Version>>,
    pub features: HashSet<FeatureName>,
}

/// Request to get manifest for a particular package.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PinnedRequest {
    pub id: RequestIdentifier,
    pub version: Version,
    pub features: HashSet<FeatureName>,
}

/// A common identifier of a request.
#[derive(Clone, Copy, Debug, Hash, PartialEq, Eq)]
pub struct RequestIdentifier {
    pub name: StrId,
    pub source: Source,
}

impl ManifestsRequest {
    pub fn new_pinned(
        source: Source,
        name: StrId,
        version: Version,
        features: HashSet<FeatureName>,
    ) -> Self {
        Self::Pinned(PinnedRequest {
            id: RequestIdentifier { name, source },
            version,
            features,
        })
    }

    pub fn new_not_pinned(
        source: Source,
        name: StrId,
        versions: Option<Vec<Version>>,
        features: HashSet<FeatureName>,
    ) -> Self {
        Self::NotPinned(NotPinnedRequest {
            id: RequestIdentifier { name, source },
            versions,
            features,
        })
    }
}

/// Type representing non-error results of a fetch.
pub enum FetchResponse {
    Success(FetchSuccess),
    Failed(FetchFailure),
}

/// Type representing the result of a successful fetch.
#[derive(Debug)]
pub enum FetchSuccess {
    Pinned(PinnedSuccess),
    NotPinned(NotPinnedSuccess),
}

/// Result of a successful fetch of a single package's manifest.
#[derive(Debug)]
pub struct PinnedSuccess {
    pub origin_id: RequestIdentifier,
    pub origin_version: Version,
    pub answer_package: PackageId,
    pub fetched_manifest: Box<Manifest>,
}

/// Result of a successful fetch of manifests of all packages from a source.
#[derive(Debug)]
pub struct NotPinnedSuccess {
    pub origin_id: RequestIdentifier,
    pub fetched_manifests: HashMap<PackageId, Box<Manifest>>,
}

/// Type representing a failed fetch.
#[derive(Debug)]
pub enum FetchFailure {
    Pinned(PinnedFailure),
    NotPinned(NotPinnedFailure),
}

/// Failed fetch of a single package's manifest.
#[derive(Debug)]
pub struct PinnedFailure {
    pub origin_id: RequestIdentifier,
    pub origin_version: Version,
}

/// Failed fetch of manifests of all packages from a source.
#[derive(Debug)]
pub struct NotPinnedFailure {
    pub origin_id: RequestIdentifier,
}

/// Type representing what action to perform for a given request.
#[derive(Debug)]
pub enum RequestAction {
    /// A fetch for such request was never made, so the fetch should be performed.
    Fetch,
    /// No need for a fetch, but further requests result from this one.
    More { requests: Vec<ManifestsRequest> },
}

impl RequestAction {
    pub fn unwrap_requests(self) -> QuackResult<Vec<ManifestsRequest>> {
        match self {
            RequestAction::Fetch => {
                qp_bail_internal!("called for manifests requests on a fetch request")
            }
            RequestAction::More { requests } => Ok(requests),
        }
    }
}

impl Default for RequestAction {
    fn default() -> Self {
        Self::More { requests: vec![] }
    }
}

impl From<Vec<ManifestsRequest>> for RequestAction {
    fn from(value: Vec<ManifestsRequest>) -> Self {
        RequestAction::More { requests: value }
    }
}
