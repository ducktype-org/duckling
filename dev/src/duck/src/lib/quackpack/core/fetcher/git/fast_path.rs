#![expect(dead_code)]
use crate::quackpack::core::fetcher::git::github_client::{GithubApiClient, GithubClient};
use crate::quackpack::core::fetcher::git::gitlab_client::{GitlabApiClient, GitlabClient};
use crate::quackpack::core::{GitReference, Manifest};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::{QuackResult, StrId};

/// Trait for comunicating with git repository servers which provide special APIs,
/// allowing us to postpone/completely omit clones.
pub trait GitFastPathExt<'duck>: Sized {
    /// Underlying client responsible for low-level API queries.
    type ApiClient;

    /// Create a new [`GitFastPathExt`], checking if the repository is supported by the client.
    /// If no, returns [`None`], if yes, returns the [`InternedUrl`], which is the base API url for the repository.
    ///
    /// Note
    /// ----
    /// Currently all the servers which we support have in common that all the requests share the same url prefix,
    /// which is returned by this function. This might change in the future / be different for different servers.
    fn new(client: &'duck Self::ApiClient, repo_url: InternedUrl) -> Option<Self>;

    /// Translate a git reference into a commit hash.
    fn get_commit_hash(&self, reference: GitReference) -> QuackResult<StrId>;

    /// Download the manifest from a repository.
    fn download_manifest(&self, commit: StrId) -> QuackResult<Manifest>;
}

pub struct GitFastPathClient<'duck> {
    gitlab: GitlabApiClient<'duck>,
    github: GithubApiClient<'duck>,
}

impl GitFastPathClient<'_> {
    pub fn try_get_github_client<'a>(
        &'a self,
        repo_url: InternedUrl,
    ) -> Option<impl GitFastPathExt<'a>> {
        GithubClient::new(&self.github, repo_url)
    }

    pub fn try_get_gitlab_client<'a>(
        &'a self,
        repo_url: InternedUrl,
    ) -> Option<impl GitFastPathExt<'a>> {
        GitlabClient::new(&self.gitlab, repo_url)
    }
}
