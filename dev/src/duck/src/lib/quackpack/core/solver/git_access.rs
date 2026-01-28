use std::path::{Path, PathBuf};

use url::Url;

use crate::{QuackResult, StrId};

pub trait GitAccess {
    fn git_path(&self, url: Url, commit: StrId) -> PathBuf;
    fn is_stored(&self, url: Url, commit: StrId) -> bool;
    fn store(&mut self, url: Url, commit: StrId, source_path: &Path) -> QuackResult<()>;
    // fn get_cached_git
}
