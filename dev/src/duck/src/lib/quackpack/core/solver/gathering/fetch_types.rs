use std::collections::{HashMap, HashSet};

use crate::quackpack::core::{
    FeatureName, Manifest, Version,
    types_common::{ExpandedPackage, InternedLocation, Package},
};

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
    pub package: Package,
    pub features: HashSet<FeatureName>,
}

/// Type respresenting the result of a successful fetch.
#[derive(Debug)]
pub enum FetchResult {
    Pinned(PinnedResult),
    NotPinned(NotPinnedResult),
}

/// Result of a fetch of a single package's manifest.
#[derive(Debug)]
pub struct PinnedResult {
    pub origin_package: Package,
    pub expanded_package: ExpandedPackage,
    pub fetched_manifest: Box<Manifest>,
}

/// Result of a fetch of manifests of all packages from a location.
#[derive(Debug)]
pub struct NotPinnedResult {
    pub origin_location: InternedLocation,
    pub fetched_manifests: HashMap<ExpandedPackage, Box<Manifest>>,
}
