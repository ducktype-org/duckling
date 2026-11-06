// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Manager of fetcher specific file locks.

use tracing::trace;

use super::types;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult, QuackResultContext};

#[derive(Debug)]
/// Manager of the fetcher specific file locks.
pub struct Locks {
    root: FileLockManager,
}

impl Locks {
    /// Filename of the per-package lock.
    const PER_PACKAGE_LOCK: &'static str = ".duck.package.lock";

    /// Create a new [`Locks`].
    pub fn new(root: FileLockManager) -> QuackResult<Self> {
        trace!(
            locks = %root.display(),
        );
        root.mkdir()?;
        Ok(Self { root })
    }

    /// Try to get a package's lock.
    pub fn lock(&self, package: types::Package, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.root
            .join(package.name)
            .join(package.version.to_string())
            .open_exclusive(Self::PER_PACKAGE_LOCK, ctx)
            .with_context(|| {
                format!(
                    "failed to acquire a lock for `{}` version {}",
                    package.name, package.version
                )
            })
    }
}
