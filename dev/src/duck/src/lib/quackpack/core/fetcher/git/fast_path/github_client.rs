//! Module for using Github's API instead of blindly cloning the full repository.
//! The whole documentation can be found here: <https://docs.github.com/en/rest>.
//!
//! Github's API:
//! -------------
//! We currently use API version 2026-03-10.
//!
//! Assume we have a repository `foo` authored by `author`.
//! The dependency on such repository is described by a url `https://github.com/author/foo`.
//! Then the base URL for getting information about this repository is `https://api.github.com/repos/author/foo`.
//! Let us call this `base_api_url`.
//!
//! We perform 2 types of queries.
//! 1. Get the commit hash for the default branch (`base_api_url/commits`) or for the commit specified by git referense (`base_api_urlcommits?sha=<reference>`).
//! 2. Download the manifest (for a given commit): `base_api_url/contents/<manifest_path>?ref=<commit_hash>`.
#![expect(dead_code)]
use std::path::Path;

use serde::Deserialize;

use crate::quackpack::core::fetcher::git::fast_path::GitFastPathExt;
use crate::quackpack::core::fetcher::git::fast_path::github_api_client::GithubApiClient;
use crate::quackpack::core::fetcher::util::http::Response;
use crate::quackpack::core::fetcher::util::http::traits_extensions::ResponseExt;
use crate::quackpack::core::{
    GitReference, Manifest, PackageLoader, ParseMode, manifest, parse_schema,
};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_url::ToUrl;
use crate::{QuackResult, QuackResultContext, StrId};

/// Client serving a git fast-path for Github repositories.
pub struct GithubClient<'duck> {
    client: &'duck GithubApiClient<'duck>,
    repo_api_url: InternedUrl,
}

impl<'duck> GitFastPathExt<'duck> for GithubClient<'duck> {
    type ApiClient = GithubApiClient<'duck>;

    fn new(client: &'duck GithubApiClient, repo_url: InternedUrl) -> Option<Self> {
        let user = repo_url.username();
        let port = repo_url.port();
        let scheme = repo_url.scheme();
        let domain = repo_url.domain()?;
        // Check that scheme is `http` or `https`.
        if scheme != "http" && scheme != "https" {
            return None;
        }
        // Check that this is a github repo.
        // Github offers enterprise self-hosted repos, but for them the API base url is created differently.
        // We can add support for those in the future.
        if domain != "github.com" {
            return None;
        }
        let path = repo_url.path();
        let mut repo_api_url = format!("{scheme}://api.{domain}/repos{path}/")
            .to_url()
            .ok()?;
        repo_api_url.set_username(user).ok()?;
        repo_api_url.set_port(port).ok()?;
        Some(Self {
            client,
            repo_api_url: repo_api_url.into(),
        })
    }

    fn get_commit_hash(&self, reference: GitReference) -> QuackResult<StrId> {
        let response = match reference {
            GitReference::Default => self.client.retrieve_a_commit(&self.repo_api_url, None)?,
            GitReference::Tag(tag) => self
                .client
                .retrieve_a_commit(&self.repo_api_url, Some(tag))?,
            GitReference::Branch(branch) => self
                .client
                .retrieve_a_commit(&self.repo_api_url, Some(branch))?,
            GitReference::Rev(commit) => self
                .client
                .retrieve_a_commit(&self.repo_api_url, Some(commit))?,
        };
        get_commit_from_response(response)
    }

    fn download_manifest(&self, commit: StrId) -> QuackResult<Manifest> {
        let response = self.client.download_file_from_commit(
            &self.repo_api_url,
            PackageLoader::MANIFEST_NAME,
            commit,
        )?;
        let deserialized_manifest = String::from_utf8(response.into_body())?;
        let manifest_schema = parse_schema(&deserialized_manifest)?;
        let manifest = manifest::parse(
            &manifest_schema,
            Path::new(""), // Dummy path.
            ParseMode::Package,
            self.client.ctx(),
        )?;
        Ok(manifest)
    }
}

/// Type representing the interesting part of the response to `/commits` requests.
/// Based on <https://docs.github.com/en/rest/commits/commits?apiVersion=2026-03-10#list-commits>.
#[derive(Deserialize)]
struct CommitResponse {
    sha: String,
}

/// Deserialize the response for requests `/commits` and get the `sha` field of the first element.
fn get_commit_from_response(response: Response) -> QuackResult<StrId> {
    let data: Vec<CommitResponse> = response
        .deserialize_json()
        .context("failed to deserialize response to CommitResponse")?;
    let base_commit = data
        .into_iter()
        .next()
        .context("got an empty list of commits in the response")?;
    Ok(base_commit.sha.into())
}

#[cfg(test)]
mod test {
    use crate::DuckContext;
    use crate::quackpack::core::fetcher::git::fast_path::GitFastPathExt;
    use crate::quackpack::core::fetcher::git::fast_path::github_api_client::GithubApiClient;
    use crate::quackpack::core::fetcher::git::fast_path::github_client::GithubClient;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn base_api_url() {
        let ctx = DuckContext::default();
        let api_client = GithubApiClient::new(&ctx);

        let repo_url = "https://github.com/foo/xd".to_url().unwrap().into();
        let github_client = GithubClient::new(&api_client, repo_url).unwrap();
        let api_url = "https://api.github.com/repos/foo/xd/".to_url().unwrap();
        assert_eq!(api_url, github_client.repo_api_url)
    }
}
