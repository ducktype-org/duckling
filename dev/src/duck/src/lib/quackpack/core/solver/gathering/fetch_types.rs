// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::collections::{HashMap, HashSet};

use crate::quackpack::core::{FeatureName, Manifest, PackageId, Source, Version};
use crate::{QuackResult, QuackResultContext, StrId, qp_bail_internal};

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
    /// Create a new pinned [`ManifestsRequest`].
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

    /// Create a new not pinned [`ManifestsRequest`].
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

    pub fn request_identifier(&self) -> RequestIdentifier {
        match self {
            Self::Pinned(pinned_request) => pinned_request.id,
            Self::NotPinned(not_pinned_request) => not_pinned_request.id,
        }
    }
}

/// Type representing non-error results of a fetch.
pub enum FetchResponse {
    Success(FetchSuccess),
    Failed(FetchFailure),
}

impl FetchResponse {
    /// Create a new pinned [`FetchResponse::Failed`].
    pub fn failed_pinned(id: RequestIdentifier, version: Version) -> Self {
        Self::Failed(FetchFailure::Pinned(PinnedFailure {
            origin_id: id,
            origin_version: version,
        }))
    }

    /// Create a new not pinned [`FetchResponse::Failed`].
    pub fn failed_not_pinned(id: RequestIdentifier) -> Self {
        Self::Failed(FetchFailure::NotPinned(NotPinnedFailure { origin_id: id }))
    }
}

/// Type representing the result of a successful fetch.
#[derive(Debug)]
pub enum FetchSuccess {
    Pinned(PinnedSuccess),
    NotPinned(NotPinnedSuccess),
}

impl FetchSuccess {
    /// Returns the latest version found in the response.
    pub fn latest_version(&self) -> QuackResult<Version> {
        match self {
            Self::NotPinned(not_pinned) => not_pinned
                .fetched_manifests
                .iter()
                .map(|m| m.0.version())
                .max()
                .with_context_internal(|| {
                    format!("response `{not_pinned:?}` to fetch without any manifests")
                }),
            Self::Pinned(pinned) => Ok(pinned.fetched_manifest.version()),
        }
    }
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
    /// Get the requests or bail internally if in [`Self::Fetch`] version.
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
