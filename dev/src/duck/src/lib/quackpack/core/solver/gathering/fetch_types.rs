use std::collections::{HashMap, HashSet};

use crate::quackpack::core::solver::types_common::{ExpandedPackage, InternedLocation};
use crate::quackpack::core::{FeatureName, Manifest, Version};

/// Type representing a request to get manifests for a single/multiple packages.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ManifestsRequest {
    Pinned(PinnedRequest),
    NotPinned(NotPinnedRequest),
}

/// Request to get manifests for all packages from a given location, satisfying given versions selector.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NotPinnedRequest {
    pub location: InternedLocation,
    pub versions: Option<Vec<Version>>,
    pub features: HashSet<FeatureName>,
}

/// Request to get manifest for a particular package.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PinnedRequest {
    pub location: InternedLocation,
    pub version: Version,
    pub features: HashSet<FeatureName>,
}

/// Type representing non-error results of a fetch.
pub enum FetchResponse {
    Success(FetchSuccess),
    Failed(FetchFailure),
}

/// Type respresenting the result of a successful fetch.
#[derive(Debug)]
pub enum FetchSuccess {
    Pinned(PinnedSuccess),
    NotPinned(NotPinnedSuccess),
}

/// Result of a successful fetch of a single package's manifest.
#[derive(Debug)]
pub struct PinnedSuccess {
    pub origin_location: InternedLocation,
    pub origin_version: Version,
    pub expanded_package: ExpandedPackage,
    pub fetched_manifest: Box<Manifest>,
}

/// Result of a successful fetch of manifests of all packages from a location.
#[derive(Debug)]
pub struct NotPinnedSuccess {
    pub origin_location: InternedLocation,
    pub fetched_manifests: HashMap<ExpandedPackage, Box<Manifest>>,
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
    pub origin_location: InternedLocation,
    pub origin_version: Version,
}

/// Failed fetch of manifests of all packages from a location.
#[derive(Debug)]
pub struct NotPinnedFailure {
    pub origin_location: InternedLocation,
}
