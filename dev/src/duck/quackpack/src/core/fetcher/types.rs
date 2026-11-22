// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Common types used in network communication.
use duck_utils::util::interned_url::InternedUrl;
use serde::{Deserialize, Serialize};

use crate::core;
use crate::core::Version;
use crate::schemas::registry;
use crate::{QuackError, StrId};

#[derive(Debug, Clone, Copy, Deserialize, Serialize)]
/// Represents exact informations required to fetch some data of a package `id` in version
/// `version` from repository at `url`.
pub struct PackageWithUrl {
    /// Name of this package.
    pub name: StrId,
    /// Version of this package.
    pub version: Version,
    /// Url pointing to a Ducknest instance with this package.
    pub url: InternedUrl,
}

impl PackageWithUrl {
    /// Convert this [`PackageWithUrl`] into a [`Package`].
    pub fn as_package(self) -> Package {
        Package {
            name: self.name,
            version: self.version,
        }
    }
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
/// This is a helper struct, used mainly for two things:
/// 1. [`SearchResult`], so we don't copy `url`s around,
/// 2. for [`UrlExt`](super::ducknest::endpoints::UrlExt) internal trait: when creating an endpoint,
///    we don't need the root `url`, it's `self`.
pub struct Package {
    /// Name of this package.
    pub name: StrId,
    /// Version of this package.
    pub version: Version,
}

impl From<&registry::Manifest> for Package {
    fn from(value: &registry::Manifest) -> Self {
        let name = &value.metadata.name;
        Self {
            name: name.into(),
            version: value.metadata.version,
        }
    }
}

#[derive(Debug)]
/// Response of the [`Fetcher::clone_from_git_to_directory`] and [`Fetcher::clone_from_git`].
///
/// [`Fetcher::clone_from_git_to_directory`]: super::Fetcher::clone_from_git_to_directory
/// [`Fetcher::clone_from_git`]: super::Fetcher::clone_from_git
pub struct ClonedPackage {
    /// Hash of the checkout'd repository.
    pub commit: StrId,
    /// Parsed package at the repository checkout'd at the
    /// [`commit`](ClonedPackage::commit).
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
pub enum FetchResponse<T> {
    /// Resource was available.
    Some(T),
    /// [`Fetcher`](super::Fetcher) was created in an offline mode.
    Offline,
}

#[derive(Debug)]
/// A marker struct indicating that we failed to initialize a client.
// NOTE: We don't derive `Error`, nor implement `From<FetchError> for QuackError`: otherwise `?`
// operator could coerce them back into a single `QuackError`, which we don't want.
pub struct InitializationError(QuackError);

impl From<InitializationError> for QuackError {
    fn from(value: InitializationError) -> Self {
        value.0
    }
}

impl From<QuackError> for InitializationError {
    fn from(value: QuackError) -> Self {
        Self(value)
    }
}

#[derive(Debug)]
/// A generic error returned by [`Fetcher`].
///
/// [`Fetcher`]: super::Fetcher
// NOTE: We don't derive `Error`, nor implement `From<FetchError> for QuackError`: otherwise `?`
// operator could coerce them back into a single `QuackError`, which we don't want.
pub enum FetchError {
    /// We failed to initialize the client.
    Initialization(InitializationError),
    /// Other error has occurred.
    Other(QuackError),
}

impl FetchError {
    /// Convert [`FetchError`] into a [`QuackError`].
    ///
    /// **IMPORTANT:** This method should be used with caution: [`FetchError`] has limited set of
    /// implemented traits, so that `?` operator doesn't convert [`FetchError`] into [`QuackError`]
    /// by mistake. You can do achieve this using this method, but be cautious.
    pub fn into_inner(self) -> QuackError {
        match self {
            Self::Initialization(e) => e.into(),
            Self::Other(e) => e,
        }
    }
}

impl From<InitializationError> for FetchError {
    fn from(value: InitializationError) -> Self {
        Self::Initialization(value)
    }
}

impl From<QuackError> for FetchError {
    fn from(value: QuackError) -> Self {
        Self::Other(value)
    }
}

pub type FetchResult<T> = Result<T, FetchError>;
pub type InitializationResult<T> = Result<T, InitializationError>;
