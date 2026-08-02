use std::path::{Path, PathBuf};

use crate::QuackResult;
use crate::quackpack::util::interned_url::InternedUrl;

/// An API required from an entity which stores packages cloned from git repositories.
pub trait GitAccess {
    /// Get a path under which a package is stored/would be stored.
    fn git_path(&self, url: InternedUrl, commit: &str) -> PathBuf;
    /// Check whether a given package is stored.
    fn is_stored(&self, url: InternedUrl, commit: &str) -> bool;
    /// Store a given package, which currently is under a given path.
    fn store(&self, url: InternedUrl, commit: &str, source_path: &Path) -> QuackResult<()>;
    /// As [`GitAccess::git_path`], but only returns the path if the package is actually stored.
    fn path_if_stored(&self, url: InternedUrl, commit: &str) -> Option<PathBuf> {
        if self.is_stored(url, commit) {
            Some(self.git_path(url, commit))
        } else {
            None
        }
    }
}
