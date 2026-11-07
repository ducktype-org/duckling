// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! A storage management of gits.
use std::path::{Path, PathBuf};

use super::paths::Storage;
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::{PackageId, Version};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::util::path_ops::{MkdirOptions, mkdir, move_tree, remove_tree};
use crate::{QuackResult, StrId};
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

    fn pkg_id(&self, url: InternedUrl, commit: StrId) -> PackageId {
        let origin = FullOrigin::for_git(url, commit);
        // !TODO: Version and name don't matter (right now). But we should pass it nevertheless.
        let identity = FullIdentity::new("mocked".into(), origin);
        PackageId::new(identity, Version::default())
    }
}

impl GitAccess for StorageGitAccess<'_> {
    fn git_path(&self, url: InternedUrl, commit: StrId) -> PathBuf {
        self.paths.pkg_dir(self.pkg_id(url, commit))
    }

    fn is_stored(&self, url: InternedUrl, commit: StrId) -> bool {
        self.paths.is_package_stored(self.pkg_id(url, commit))
    }

    fn store(&self, url: InternedUrl, commit: StrId, source_path: &Path) -> QuackResult<()> {
        let pkg_id = self.pkg_id(url, commit);
        let dir = self.paths.pkg_dir(pkg_id);
        if dir.exists() {
            remove_tree(&dir)?;
        }
        if let Some(parent) = dir.parent() {
            mkdir(parent, MkdirOptions::WithParents)?;
        }
        move_tree(source_path, &dir)?;
        self.paths.mark_as_stored(pkg_id)?;
        Ok(())
    }
}
