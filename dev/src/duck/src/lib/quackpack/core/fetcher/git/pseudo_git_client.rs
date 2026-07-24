use crate::quackpack::core::{GitReference, Manifest};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::{QuackResult, StrId};

#[expect(dead_code)]
/// Trait for comunicating with git repository servers which provide special APIs,
/// allowing us to postpone/completely omit clones.
pub trait PseudoGitClient {
    /// Whether the client supports the given repository domain.
    /// If no, returns [`None`], if yes, returns the [`InternedUrl`], which is the base API url for the repository.
    ///
    /// Note
    /// ----
    /// Currently all the servers which we support have in common that all the requests share the same url prefix,
    /// which is returned by this function. This might change in the future / be different for different servers.
    fn get_api_url(repo_url: InternedUrl) -> Option<InternedUrl>;

    /// Translate a git reference into a commit hash.
    fn get_commit_hash(&self, api_url: InternedUrl, reference: GitReference) -> QuackResult<StrId>;

    /// Download the manifest from a repository.
    fn download_manifest(&self, api_url: InternedUrl, commit: StrId) -> QuackResult<Manifest>;
}
