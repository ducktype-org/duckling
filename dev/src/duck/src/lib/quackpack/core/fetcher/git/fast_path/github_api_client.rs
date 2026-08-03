//! Module for using parts of Github's API.
//! The whole documentation can be found here: <https://docs.github.com/en/rest?apiVersion=2026-03-10>.
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
//! 1. Get the commit hash for the default branch (`base_api_url/commits`) or for the commit specified by git referense (`base_api_url/commits?sha=<reference>`).
//! 2. Download the manifest (for a given commit): `base_api_url/contents/<manifest_path>?ref=<commit_hash>`.use http::{HeaderName, HeaderValue, header};
use http::{HeaderName, HeaderValue, header};
use serde::Deserialize;
use tracing::debug;
use url::Url;

use crate::quackpack::core::fetcher::http::HttpClient;
use crate::quackpack::core::fetcher::util::http::traits_extensions::ResponseExt;
use crate::quackpack::core::fetcher::util::http::{Request, Response, defaults};
use crate::quackpack::util::to_url::ToUrl;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

/// Client for performing requests to Github repositories.
pub struct GithubApiClient<'duck> {
    client: HttpClient<'duck>,
}

impl<'duck> GithubApiClient<'duck> {
    /// Create a new [`GithubClient`] instance.
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self {
            client: HttpClient::new(ctx),
        }
    }

    /// Get the base api for requests for this repository.
    /// This will be the prefix for all the requests for this repository.
    pub fn get_api_url(repo_url: &Url) -> Option<Url> {
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
        // Remove any `.git` in the end of the path.
        let path_trimmed = if let Some(path) = path.strip_suffix(".git") {
            path
        } else {
            path
        };
        let mut repo_api_url = format!("{scheme}://api.{domain}/repos{path_trimmed}/")
            .to_url()
            .ok()?;
        repo_api_url.set_username(user).ok()?;
        repo_api_url.set_port(port).ok()?;
        Some(repo_api_url)
    }

    /// Docs: <https://docs.github.com/en/rest/commits/commits?apiVersion=2026-03-10#list-commits>.
    /// Get list of information about commits, starting from the default branch (if `reference` is [`None`])
    /// or the commit specified by branch, tag or 1-byte commit id.
    /// Used to get the full commit identifier.
    pub fn retrieve_a_commit(
        &self,
        repo_api_url: &Url,
        reference: Option<StrId>,
    ) -> QuackResult<StrId> {
        let mut url = repo_api_url.join("commits")?;
        if let Some(reference) = reference {
            url.query_pairs_mut().append_pair("sha", &reference);
        }
        url.query_pairs_mut().append_pair("per_page", "1");
        let response = self.request(&url)?;
        get_commit_from_response(response)
    }

    /// Docs: <https://docs.github.com/en/rest/repos/contents?apiVersion=2026-03-10#get-repository-content>.
    /// Download a raw file from the repository at specific commit.
    /// Used to download the manifest.
    pub fn download_file_from_commit(
        &self,
        repo_api_url: &Url,
        path_to_file: &str,
        commit: StrId,
    ) -> QuackResult<String> {
        let mut url = repo_api_url.join("contents/")?.join(path_to_file)?;
        url.query_pairs_mut().append_pair("ref", &commit);
        let response = self.request(&url)?;
        Ok(String::from_utf8(response.into_body())?)
    }

    /// Create a `GET` request for the specified `url`.
    fn request(&self, url: &Url) -> QuackResult<Response> {
        let mut request = Self::create_http_request(url, http::Method::GET, vec![])?;
        request
            .headers_mut()
            .entry(header::PRAGMA)
            .or_insert(defaults::NO_VALUE);
        // Add header for the right API version.
        request.headers_mut().insert(
            HeaderName::from_static("X-GitHub-Api-Version"),
            HeaderValue::from_static("2026-03-10"),
        );
        request.headers_mut().insert(
            header::ACCEPT,
            HeaderValue::from_static("application/vnd.github.raw"),
        );
        self.client.request(request)
    }

    /// Helper for [`Self::request`].
    fn create_http_request(url: &Url, method: http::Method, body: Vec<u8>) -> QuackResult<Request> {
        debug!(%method, %url, ?body, "building a request");
        http::Request::builder()
            .uri(url.as_str())
            .method(&method)
            .body(body)
            .with_context_internal(|| {
                format!("failed to build an HTTP request: url: `{url}`, method: `{method:#?}`")
            })
    }

    /// Get the underlying [`DuckContext`].
    pub fn ctx(&self) -> &DuckContext {
        self.client.ctx()
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
    use crate::quackpack::core::fetcher::git::fast_path::github_api_client::GithubApiClient;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn base_api_url() {
        let repo_url = "https://github.com/foo/xd".to_url().unwrap();
        let api_url = "https://api.github.com/repos/foo/xd/".to_url().unwrap();
        assert_eq!(api_url, GithubApiClient::get_api_url(&repo_url).unwrap())
    }

    #[test]
    fn remove_dot_git() {
        let repo_url = "https://github.com/foo/xd.git".to_url().unwrap();
        let api_url = "https://api.github.com/repos/foo/xd/".to_url().unwrap();
        assert_eq!(api_url, GithubApiClient::get_api_url(&repo_url).unwrap())
    }
}
