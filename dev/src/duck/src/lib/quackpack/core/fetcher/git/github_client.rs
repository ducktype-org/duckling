//! Module for using Github's API instead of blindly cloning the full repository.
//! The whole documentation can be found here: `https://docs.github.com/en/rest`.
//!
//! Github's API:
//! -------------
//! Assume we have a repository `foo` authored by `author`.
//! The dependency on such repository is described by a url `https://github.com/author/foo`.
//! Then the base URL for getting information about this repository is `https://api.github.com/repos/author/foo`.
//! Let us call this `base_api_url`.
//!
//! We perform 3 types of queries.
//! 1. Get the name of the default branch: `base_api_url`.
//! 2. Get the commit hash for a given git reference: `base_api_url/git/ref/tags/<tag>` and `base_api_url/git/ref/heads/<branch_name>`.
//! 3. Download the manifest (for a given commit): `base_api_url/contents/<manifest_path>?ref=<commit_hash>`.
#![expect(dead_code)]
use std::path::Path;

use http::{HeaderValue, header};
use serde::Deserialize;
use tracing::debug;
use url::Url;

use crate::quackpack::core::fetcher::git::pseudo_git_client::PseudoGitClient;
use crate::quackpack::core::fetcher::http::HttpClient;
use crate::quackpack::core::fetcher::util::http::traits_extensions::ResponseExt;
use crate::quackpack::core::fetcher::util::http::{Request, Response, defaults};
use crate::quackpack::core::{
    GitReference, Manifest, PackageLoader, ParseMode, manifest, parse_schema,
};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_url::ToUrl;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

/// Client for performing requests to Github repositories.
pub struct GithubClient<'duck> {
    client: HttpClient<'duck>,
}

impl<'duck> GithubClient<'duck> {
    /// Create a new [`GithubClient`] instance.
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self {
            client: HttpClient::new(ctx),
        }
    }

    /// Get the underlying [`DuckContext`].
    pub fn ctx(&self) -> &DuckContext {
        self.client.ctx()
    }
}

impl PseudoGitClient for GithubClient<'_> {
    fn get_api_url(repo_url: InternedUrl) -> Option<InternedUrl> {
        let user = repo_url.username();
        let port = repo_url.port();
        let scheme = repo_url.scheme();
        let domain = repo_url.domain()?;
        // Check that scheme is `http` or `https`.
        if !(scheme == "http") && !(scheme == "https") {
            return None;
        }
        // Check that this is a github repo.
        if !domain.starts_with("github") {
            return None;
        }
        let path = repo_url.path();
        let mut api_url = format!("{scheme}://api.{domain}/repos{path}/")
            .to_url()
            .ok()?;
        api_url.set_username(user).ok()?;
        api_url.set_port(port).ok()?;
        Some(api_url.into())
    }

    fn get_commit_hash(&self, api_url: InternedUrl, reference: GitReference) -> QuackResult<StrId> {
        match reference {
            GitReference::Default => {
                // Docs: `https://docs.github.com/en/rest/repos/repos?apiVersion=2026-03-10#get-a-repository`.
                // `api_url` has trailing slash, so that `Url::join` works.
                // But Github does not support requests for that url, so here we trim the trailing slash.
                let mut tmp_url = api_url.as_url().clone();
                let path = api_url.path();
                let new_path = path.trim_end_matches('/');
                tmp_url.set_path(new_path);
                let request = create_get_request(&tmp_url)?;
                let response = self.client.request(request)?;
                let default_branch = get_default_branch_from_response(response)?;
                self.get_commit_hash(api_url, GitReference::Branch(default_branch))
            }
            GitReference::Tag(tag) => {
                // Docs: `https://docs.github.com/en/rest/git/tags?apiVersion=2026-03-10#get-a-tag.
                let url = api_url.join("git/ref/tags/")?.join(&tag)?;
                let request = create_get_request(&url)?;
                let response = self.client.request(request)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Branch(branch) => {
                // Docs: `https://docs.github.com/en/rest/branches/branches?apiVersion=2026-03-10#get-a-branch`.
                let url = api_url.join("git/ref/heads/")?.join(&branch)?;
                let request = create_get_request(&url)?;
                let response = self.client.request(request)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Rev(commit) => Ok(commit),
        }
    }

    fn download_manifest(&self, api_url: InternedUrl, commit: StrId) -> QuackResult<Manifest> {
        // Docs: `https://docs.github.com/en/rest/repos/contents?apiVersion=2026-03-10#get-repository-content`.
        let mut url = api_url.join(&format!("contents/{}", PackageLoader::MANIFEST_NAME,))?;
        url.set_query(Some(&format!("ref={}", commit)));
        let mut request = create_get_request(&url)?;
        request.headers_mut().insert(
            header::ACCEPT,
            HeaderValue::from_static("application/vnd.github.raw"),
        );
        let response = self.client.request(request)?;
        let deserialized_manifest = String::from_utf8(response.into_body())?;
        let manifest_schema = parse_schema(&deserialized_manifest)?;
        let manifest = manifest::parse(
            &manifest_schema,
            Path::new(""), // Dummy path.
            ParseMode::Package,
            self.ctx(),
        )?;
        Ok(manifest)
    }
}

/// Type representing the interesting part of the response to `.../branches/<branch>` and `.../tags/<tag>` requests.
/// Based on `https://docs.github.com/en/rest/branches/branches?apiVersion=2026-03-10#get-a-branch` and
/// `https://docs.github.com/en/rest/git/tags?apiVersion=2026-03-10#get-a-tag`.
#[derive(Deserialize)]
struct CommitResponse {
    pub object: CommitResponseSha,
}

/// Helper for [`CommitResponse`].
#[derive(Deserialize)]
struct CommitResponseSha {
    pub sha: String,
}

/// Deserialize the response for requests `.../branches/<branch>` and `.../tags/<tag>` and get the `object.sha` field.
fn get_commit_from_response(response: Response) -> QuackResult<StrId> {
    let data: CommitResponse = response
        .deserialize_json()
        .context("failed to deserialize response to CommitResponse")?;
    Ok(data.object.sha.into())
}

/// Type representing the interesting part of the response to get repo request.
/// Based on `https://docs.github.com/en/rest/repos/repos?apiVersion=2026-03-10#get-a-repository`.
#[derive(Deserialize)]
struct DefaultBranchResponse {
    default_branch: String,
}

/// Deserialize the response for request to the url of the repository and get the `default_branch` field.
fn get_default_branch_from_response(response: Response) -> QuackResult<StrId> {
    let data: DefaultBranchResponse = response
        .deserialize_json()
        .context("failed to deserialize response to DefaultBranchResponse")?;
    Ok(data.default_branch.into())
}

fn create_get_request(url: &Url) -> QuackResult<Request> {
    let mut request = create_http_request(url, http::Method::GET, vec![])?;
    request
        .headers_mut()
        .entry(header::PRAGMA)
        .or_insert(defaults::NO_VALUE);
    Ok(request)
}

fn create_http_request(url: &Url, method: http::Method, body: Vec<u8>) -> QuackResult<Request> {
    debug!(%method, %url, "making an `{method}` request for `{url}`");
    http::Request::builder()
        .uri(url.as_str())
        .method(method)
        .body(body)
        .context_internal("failed to build an HTTP request")
}

#[cfg(test)]
mod test {
    use crate::quackpack::core::fetcher::git::github_client::GithubClient;
    use crate::quackpack::core::fetcher::git::pseudo_git_client::PseudoGitClient;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn base_api_url() {
        let repo_url = "https://github.com/foo/xd".to_url().unwrap().into();
        let api_url = "https://api.github.com/repos/foo/xd/".to_url().unwrap();
        assert_eq!(api_url, GithubClient::get_api_url(repo_url).unwrap())
    }
}
