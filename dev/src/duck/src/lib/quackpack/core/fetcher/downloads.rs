// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Manager of the fetcher's `downloads/` directory.

use std::path::PathBuf;

use tracing::{debug, trace};

use super::types;
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
    /// Filename of the default package's compressed source.
    const DEFAULT_BLOB_FILENAME: &'static str = "source.tar.gz";

    /// Create a new [`Downloads`].
    pub fn new(root: FileLockManager) -> QuackResult<Self> {
        root.mkdir()?;
        trace!(
            downloads = %root.display(),
        );
        Ok(Self { root })
    }

    /// Try to get a cached package.
    pub fn get(
        &self,
        package: types::PackageWithUrl,
        ctx: &DuckContext,
    ) -> QuackResult<DownloadedEntry> {
        let destination = self
            .root
            .join(package.url.as_str())
            .join(package.name)
            .join(package.version.to_string());

        let blob_path = destination
            .not_locked_path()
            .join(Self::DEFAULT_BLOB_FILENAME);

        if blob_path.exists() {
            debug!(target: "duck::ducknest", "blob cache hit at `{}`", blob_path.display());
            return Ok(DownloadedEntry::Present(blob_path));
        }

        let locked = destination.open_exclusive(Self::DEFAULT_BLOB_FILENAME, ctx)?;
        Ok(DownloadedEntry::Vacant(locked))
    }
}
