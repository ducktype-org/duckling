use std::path::{Path, PathBuf};

use url::Url;

use crate::{QuackResult, StrId};

pub trait GitAccess {
    fn git_path(&self, url: Url, commit: StrId) -> PathBuf;
    fn is_stored(&self, url: Url, commit: StrId) -> bool;
    fn store(&self, url: Url, commit: StrId, source_path: &Path) -> QuackResult<()>;
    fn path_if_stored(&self, url: Url, commit: StrId) -> Option<PathBuf> {
        if self.is_stored(url.clone(), commit) {
            Some(self.git_path(url, commit))
        } else {
            None
        }
    }
}
