use std::path::{Path, PathBuf};

use url::Url;

use crate::{
    QuackResult, StrId,
    quackpack::core::git_access::GitAccess,
    util_common::path_ops_ext::{MkdirOptions, PathOpsExt},
};

use super::package_id::{GitId, PackageId};

use super::paths::Storage;
#[derive(Debug, Clone, Copy)]
pub struct StorageGitAccess<'paths> {
    paths: &'paths Storage,
}

impl<'paths> StorageGitAccess<'paths> {
    pub fn new(paths: &'paths Storage) -> Self {
        Self { paths }
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

    fn store(&self, url: Url, commit: StrId, source_path: &Path) -> QuackResult<()> {
        let id = PackageId::Git(GitId::new(url, commit));
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
