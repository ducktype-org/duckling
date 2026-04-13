//! A storage management of gits.
use std::path::{Path, PathBuf};

use url::Url;

use crate::{
    QuackResult, StrId,
    quackpack::core::solver::git_access::GitAccess,
    util::path_ops_ext::{MkdirOptions, PathOpsExt},
};

use super::package_id::GitId;

use super::paths::Storage;
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
    fn git_path(&self, url: Url, commit: StrId) -> PathBuf {
        self.paths.pkg_dir(&GitId::new(url, commit).into())
    }

    fn is_stored(&self, url: Url, commit: StrId) -> bool {
        self.paths
            .is_package_stored(&GitId::new(url, commit).into())
    }

    fn store(&mut self, url: Url, commit: StrId, source_path: &Path) -> QuackResult<()> {
        let id = GitId::new(url, commit).into();
        let dir = self.paths.pkg_dir(&id);
        if dir.exists() {
            dir.rmtree()?;
        }
        if let Some(parent) = dir.parent() {
            parent.mkdir(MkdirOptions::WithParents)?;
        }
        source_path.rename_to(&dir)?;
        Ok(())
    }
}
