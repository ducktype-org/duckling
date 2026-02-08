use std::{
    collections::HashMap,
    path::{Path, PathBuf},
};

use rustvil::fs::{MkdirOptions, PathExt};
use url::Url;

use crate::{
    QuackResult, QuackResultContext, StrId,
    quackpack::core::{Git, git_access::GitAccess},
};

use super::package_id::{GitId, PackageId};

use super::paths::StoragePaths;
#[derive(Debug)]
pub struct StorageGitAccess<'paths> {
    paths: &'paths StoragePaths,
    _cached: HashMap<Git, GitId>,
}

impl<'paths> StorageGitAccess<'paths> {
    pub fn new(paths: &'paths StoragePaths, cached: HashMap<Git, GitId>) -> Self {
        Self {
            paths,
            _cached: cached,
        }
    }
}

impl<'paths> GitAccess for StorageGitAccess<'paths> {
    fn git_path(&self, url: Url, commit: StrId) -> PathBuf {
        self.paths.pkg_dir(&PackageId::Git(GitId::new(url, commit)))
    }

    fn is_stored(&self, url: Url, commit: StrId) -> bool {
        self.paths
            .is_package_stored(&PackageId::Git(GitId::new(url, commit)))
    }

    fn store(&mut self, url: Url, commit: StrId, source_path: &Path) -> QuackResult<()> {
        let id = PackageId::Git(GitId::new(url, commit));
        let dir = self.paths.pkg_dir(&id);
        if dir.exists() {
            dir.rmtree()
                .with_context(|| format!("failed to remove directory `{}`", dir.display()))?;
        }
        if let Some(parent) = dir.parent() {
            parent
                .mkdir(MkdirOptions::WithParents)
                .with_context(|| format!("failed to create directory `{}`", parent.display()))?;
        }
        source_path.rename_to(&dir).with_context(|| {
            format!(
                "failed to rename `{}` to `{}`",
                source_path.display(),
                dir.display()
            )
        })?;
        Ok(())
    }
}
