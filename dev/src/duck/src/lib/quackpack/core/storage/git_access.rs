// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! A storage management of gits.
use std::path::{Path, PathBuf};

use super::paths::Storage;
use crate::QuackResult;
use crate::quackpack::core::PackageId;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::util::path_ops::{MkdirOptions, mkdir, move_tree, remove_tree};
#[derive(Debug, Clone, Copy)]
/// An implementation of [`GitAccess`].
pub struct StorageGitAccess<'paths> {
    paths: &'paths Storage,
}

impl<'paths> StorageGitAccess<'paths> {
    /// Create a new [`StorageGitAccess`].
    pub fn new(paths: &'paths Storage) -> Self {
        Self { paths }
    }
}

impl GitAccess for StorageGitAccess<'_> {
    fn git_path(&self, package: PackageId) -> PathBuf {
        package.debug_assert_git_package();
        self.paths.pkg_dir(package)
    }

    fn is_stored(&self, package: PackageId) -> bool {
        package.debug_assert_git_package();
        self.paths.is_package_stored(package)
    }

    fn store(&self, package: PackageId, source_path: &Path) -> QuackResult<()> {
        package.debug_assert_git_package();
        let dir = self.paths.pkg_dir(package);
        if dir.exists() {
            remove_tree(&dir)?;
        }
        if let Some(parent) = dir.parent() {
            mkdir(parent, MkdirOptions::WithParents)?;
        }
        move_tree(source_path, &dir)?;
        self.paths.mark_as_stored(package)?;
        Ok(())
    }
}
