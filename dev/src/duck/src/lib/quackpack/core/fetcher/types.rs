//! Common types used in network communication.
use serde::{Deserialize, Serialize};
use url::Url;

use crate::StrId;
use crate::quackpack::core;
use crate::quackpack::core::Version;
use crate::quackpack::schemas::registry;

#[derive(Clone, Deserialize, Serialize)]
/// Represents exact informations required to fetch some data of a package `id` in version
/// `version` from repository at `url`.
pub struct PackageWithUrl {
    /// Name of this package.
    pub id: StrId,
    /// Version of this package.
    pub version: Version,
    /// Url pointing to a Ducknest instance with this package.
    pub url: Url,
}

impl std::fmt::Debug for PackageWithUrl {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("PackageWithUrl")
            .field("id", &self.id)
            .field("version", &self.version)
            .field("url", &self.url.as_str())
            .finish()
    }
}

#[derive(Clone, Debug, Deserialize, Serialize)]
/// This is a helper struct, used mainly for two things:
/// 1. [`SearchResult`], so we don't copy `url`s around,
/// 2. for [`UrlExt`](super::ducknest::endpoints::UrlExt) internal trait: when creating an endpoint,
///    we don't need the root `url`, it's `self`.
pub struct Package {
    /// Name of this package.
    pub id: StrId,
    /// Version of this package.
    pub version: Version,
}

impl From<&PackageWithUrl> for Package {
    fn from(value: &PackageWithUrl) -> Self {
        Self {
            id: value.id,
            version: value.version,
        }
    }
}

impl From<PackageWithUrl> for Package {
    fn from(value: PackageWithUrl) -> Self {
        Self::from(&value)
    }
}

impl From<&registry::Manifest> for Package {
    fn from(value: &registry::Manifest) -> Self {
        let name = &value.metadata.name;
        Self {
            id: name.into(),
            version: value.metadata.version,
        }
    }
}

#[derive(Debug)]
/// Response of the [`GitClient::clone_blocking`](super::git::GitClient::clone_blocking).
pub struct GitCloneResponse {
    /// Hash of the checkout'd repository.
    pub commit_hash: StrId,
    /// Parsed package at the repository checkout'd at the
    /// [`commit_hash`](GitCloneResponse::commit_hash).
    pub package: core::Package,
}

#[derive(Debug, Deserialize, Serialize)]
/// Response for getting multimetadata from a registry.
pub struct MultiMetadata {
    /// All published versions of the package.
    pub packages_metadata: Vec<registry::Manifest>,
}

#[derive(Debug, Deserialize)]
/// Response for searching for a package in a registry.
pub struct SearchResult {
    /// All matched packages.
    pub result: Vec<Package>,
}

#[derive(Debug)]
/// General fetcher response.
pub enum FetcherResponse<T> {
    /// Resource was available.
    Some(T),
    /// [`Fetcher`](super::Fetcher) was created in an offline mode.
    Offline,
}
