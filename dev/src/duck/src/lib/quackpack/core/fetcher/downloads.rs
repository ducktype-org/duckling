// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Manager of the fetcher's `downloads/` directory.
//!
//! Each file is downloaded to `downloads/<host>-<hash>/<name>-<version>.tar.gz`.

use std::path::PathBuf;

use tracing::{debug, trace};

use super::types;
use crate::quackpack::core::short_name_for_host;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult};

#[derive(Debug)]
/// Manager of the downloads cache in QuackPack.
pub struct Downloads {
    root: FileLockManager,
}

#[derive(Debug)]
/// Cached downloads entry type.
pub enum DownloadedEntry {
    /// Entry was cached, it can be accessed at this path.
    Present(PathBuf),
    /// Entry was not present. It should be stored in the given file.
    Vacant(LockedFile),
}

impl Downloads {
    /// Extension of the default package's compressed source.
    const EXTENSION: &'static str = ".tar.gz";

    /// Create a new [`Downloads`].
    pub fn new(root: FileLockManager) -> QuackResult<Self> {
        root.mkdir()?;
        trace!(
            downloads = %root.display(),
        );
        Ok(Self { root })
    }

    /// Try to get a cached package.
    ///
    /// Per package lock ([`Locks::lock`]) should be held while performing this operation.
    ///
    /// [`Locks::lock`]: super::locks::Locks::lock
    pub fn get(
        &self,
        package: types::PackageWithUrl,
        ctx: &DuckContext,
    ) -> QuackResult<DownloadedEntry> {
        let id = short_name_for_host(&package.url);
        let filename = format!("{}-{}{}", package.name, package.version, Self::EXTENSION);
        let destination = self.root.join(id);

        let blob_path = destination.not_locked_path().join(&filename);

        if blob_path.exists() {
            debug!("blob cache hit at `{}`", blob_path.display());
            return Ok(DownloadedEntry::Present(blob_path));
        }

        let locked = destination.open_exclusive(filename, ctx)?;
        Ok(DownloadedEntry::Vacant(locked))
    }
}
