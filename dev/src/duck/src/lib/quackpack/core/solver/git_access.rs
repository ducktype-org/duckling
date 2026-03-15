use std::path::{Path, PathBuf};

use url::Url;

use crate::{QuackResult, StrId};

/// An API required from an entity which stores packages cloned from git repositories.
pub trait GitAccess {
    /// Get a path under which a package is stored/would be stored.
    fn git_path(&self, url: Url, commit: StrId) -> PathBuf;
    /// Check whether a given package is stored.
    fn is_stored(&self, url: Url, commit: StrId) -> bool;
    /// Store a given package, which currently is under a given path.
    fn store(&mut self, url: Url, commit: StrId, source_path: &Path) -> QuackResult<()>;
    /// As [`GitAccess::git_path`], but only returns the path if the package is actually stored.
    fn path_if_stored(&self, url: Url, commit: StrId) -> Option<PathBuf> {
        if self.is_stored(url.clone(), commit) {
            Some(self.git_path(url, commit))
        } else {
            None
        }
    }
}
