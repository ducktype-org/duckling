// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::{Path, PathBuf};

use crate::QuackResult;
use crate::quackpack::core::PackageId;

/// An API required from an entity which stores packages cloned from git repositories.
pub trait GitAccess {
    /// Get a path under which a package is stored/would be stored.
    fn git_path(&self, package: PackageId) -> PathBuf;
    /// Check whether a given package is stored.
    fn is_stored(&self, package: PackageId) -> bool;
    /// Store a given package, which currently is under a given path.
    fn store(&self, package: PackageId, source_path: &Path) -> QuackResult<()>;
    /// As [`GitAccess::git_path`], but only returns the path if the package is actually stored.
    fn path_if_stored(&self, package: PackageId) -> Option<PathBuf> {
        if self.is_stored(package) {
            Some(self.git_path(package))
        } else {
            None
        }
    }
}
