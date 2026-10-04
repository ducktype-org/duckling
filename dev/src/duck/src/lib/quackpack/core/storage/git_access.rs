//! A storage management of gits.
use std::path::{Path, PathBuf};

use super::paths::Storage;
use crate::QuackResult;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::util::path_ops_ext::{MkdirOptions, PathOpsExt};
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
    fn git_path(&self, url: InternedUrl, commit: &str) -> PathBuf {
        self.paths.git_dir(url, commit)
    }

    fn is_stored(&self, url: InternedUrl, commit: &str) -> bool {
        self.paths.is_stored_git(url, commit)
    }

    fn store(&self, url: InternedUrl, commit: &str, source_path: &Path) -> QuackResult<()> {
        let dir = self.paths.git_dir(url, commit);
        if dir.exists() {
            dir.rmtree()?;
        }
        if let Some(parent) = dir.parent() {
            parent.mkdir(MkdirOptions::WithParents)?;
        }
        source_path.rename_to(&dir)?;
        self.paths.mark_git_stored(url, commit)?;
        Ok(())
    }
}
