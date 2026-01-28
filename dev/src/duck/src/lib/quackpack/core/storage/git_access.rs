use std::{
    collections::HashMap,
    path::{Path, PathBuf},
};

use rustvil::fs::{MkdirOptions, PathExt};
use url::Url;

use crate::{
    QuackResult, StrId,
    quackpack::core::{Git, solver::types::GitAccess},
};

use super::package_id::{GitId, PackageId};

use super::paths::StoragePaths;
#[derive(Debug)]
pub struct StorageGitAccess<'paths> {
    paths: &'paths StoragePaths,
    cached: HashMap<Git, GitId>,
}

impl<'paths> StorageGitAccess<'paths> {
    pub fn new(paths: &'paths StoragePaths, cached: HashMap<Git, GitId>) -> Self {
        Self { paths, cached }
    }
}

impl<'paths> GitAccess for StorageGitAccess<'paths> {
    fn git_path(&self, url: Url, commit: StrId) -> PathBuf {
        self.paths.pkg_dir(&PackageId::Git(GitId { url, commit }))
    }

    fn is_stored(&self, url: Url, commit: StrId) -> bool {
        self.paths
            .is_package_stored(&PackageId::Git(GitId { url, commit }))
    }

    fn store(&mut self, url: Url, commit: StrId, source_path: &Path) -> QuackResult<()> {
        let id = PackageId::Git(GitId { url, commit });
        let dir = self.paths.pkg_dir(&id);
        if dir.exists() {
            dir.rmtree()?;
        }
        if let Some(parent) = dir.parent() {
            parent.mkdir(MkdirOptions::WithParents)?;
        }
        source_path.rename_to(dir)?;
        Ok(())
    }
}
