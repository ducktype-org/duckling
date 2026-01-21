use std::path::Path;

use crate::StrId;

pub trait GitAccess {
    fn git_path(&self, url: StrId, commit: StrId) -> &Path;
    fn is_stored(&self, url: StrId, commit: StrId) -> bool;
    fn store(&mut self, url: StrId, commit: StrId, sorce_path: &Path);
    // fn get_cached_git
}
