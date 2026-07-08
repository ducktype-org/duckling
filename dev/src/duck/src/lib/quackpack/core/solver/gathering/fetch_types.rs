use std::collections::{HashMap, HashSet};

use crate::{
    StrId,
    quackpack::{
        core::{FeatureName, Manifest, Source, Version, full_identity::FullIdentity},
        util::with_version::WithVersion,
    },
};

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
    pub answer_package: WithVersion<FullIdentity>,
    pub fetched_manifest: Box<Manifest>,
}

/// Result of a successful fetch of manifests of all packages from a source.
#[derive(Debug)]
pub struct NotPinnedSuccess {
    pub origin_id: RequestIdentifier,
    pub fetched_manifests: HashMap<WithVersion<FullIdentity>, Box<Manifest>>,
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
