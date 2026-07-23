use crate::quackpack::core::{GitReference, Manifest};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::{QuackResult, StrId};

#[allow(dead_code)]
pub trait PseudoGitClient {
    /// Translate a git reference into a commit hash.
    fn get_commit_hash(&self, url: InternedUrl, reference: GitReference) -> QuackResult<StrId>;

    /// Download the manifest from a repository.
    fn download_manifest(&self, url: InternedUrl, commit: StrId) -> QuackResult<Manifest>;
}
