// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Manager of fetcher specific file locks.
//!
//! Each package is locked by `locks/<host>-<hash>/<name>-<version>.lock`.

use tracing::trace;

use super::types;
use crate::quackpack::core::short_name_for_host;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult, QuackResultContext};

#[derive(Debug)]
/// Manager of the fetcher specific file locks.
pub struct Locks {
    root: FileLockManager,
}

impl Locks {
    /// Extension of the per-package lock.
    const EXTENSION: &'static str = ".lock";

    /// Create a new [`Locks`].
    pub fn new(root: FileLockManager) -> QuackResult<Self> {
        trace!(
            locks = %root.display(),
        );
        root.mkdir()?;
        Ok(Self { root })
    }

    /// Try to get a package's lock.
    pub fn lock(
        &self,
        package: types::PackageWithUrl,
        ctx: &DuckContext,
    ) -> QuackResult<LockedFile> {
        let id = short_name_for_host(&package.url);
        let filename = format!("{}-{}{}", package.name, package.version, Self::EXTENSION);
        self.root
            .join(id)
            .open_exclusive(filename, ctx)
            .with_context(|| {
                format!(
                    "failed to acquire a lock for `{}` version {}",
                    package.name, package.version
                )
            })
    }
}
