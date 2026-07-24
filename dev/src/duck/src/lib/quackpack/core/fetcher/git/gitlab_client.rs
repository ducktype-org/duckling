//! Module for using Gitlab's API instead of blindly cloning the full repository.
//!
//! Gitlab's API:
//! -------------
//! Assume we have a repository `foo` authored by `author`.
//! The dependency on such repository is described by a url `https://gitlab.com/author/foo`.
//! Then the base URL for getting information about this repository is `https://gitlab.com/api/v4/projects/author%2Ffoo`.
//! Let us call this `base_api_url`.
//!
//! We perform 3 types of queries.
//! 1. Get the name of the default branch: `base_api_url`.
//! 2. Get the commit hash for a given git reference: `base_api_url/repository/tags/<tag>` and `base_api_url/repository/heads/<branch_name>`.
//! 3. Download the manifest (for a given commit): `base_api_url/repository/files/<manifest_path>/raw?ref=<commit_hash>`.
#![expect(dead_code)]
use std::path::Path;

use http::{HeaderValue, header};
use percent_encoding::{AsciiSet, CONTROLS, utf8_percent_encode};
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

/// Characters to encode in Gitlab servers' urls.
const PATH_ENCODE_SET: &AsciiSet = &CONTROLS
    .add(b' ')
    .add(b'"')
    .add(b'#')
    .add(b'<')
    .add(b'>')
    .add(b'?')
    .add(b'`')
    .add(b'{')
    .add(b'}')
    .add(b'/');

/// Client for performing requests to Gitlab repositories.
pub struct GitlabClient<'duck> {
    client: HttpClient<'duck>,
}

impl<'duck> GitlabClient<'duck> {
    /// Create a new [`GitlabClient`] instance.
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

impl PseudoGitClient for GitlabClient<'_> {
    fn get_api_url(repo_url: InternedUrl) -> Option<InternedUrl> {
        let scheme = repo_url.scheme();
        let domain = repo_url.domain()?;
        // Check that this is a gitlab repo.
        if !domain.starts_with("gitlab") {
            return None;
        }
        // `path()` adds `/` in the beginning.
        let path: String = repo_url.path().chars().skip(1).collect();
        let path_encoded = utf8_percent_encode(&path, PATH_ENCODE_SET);
        let api_url = format!("{scheme}://{domain}/api/v4/projects/{path_encoded}/")
            .to_url()
            .ok()?;
        Some(api_url.into())
    }

    fn get_commit_hash(&self, api_url: InternedUrl, reference: GitReference) -> QuackResult<StrId> {
        match reference {
            GitReference::Default => {
                // Docs: `https://docs.gitlab.com/api/projects/#retrieve-a-project`.
                let request = create_get_request(&api_url)?;
                let response = self.client.request(request)?;
                let default_branch = get_default_branch_from_response(response)?;
                self.get_commit_hash(api_url, GitReference::Branch(default_branch))
            }
            GitReference::Tag(tag) => {
                // Docs: `https://docs.gitlab.com/api/tags/#retrieve-a-single-repository-tag`.
                let url = api_url.join("repository/tags/")?.join(&tag)?;
                let request = create_get_request(&url)?;
                let response = self.client.request(request)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Branch(branch) => {
                // Docs: `https://docs.gitlab.com/api/branches/#retrieve-a-repository-branch`.
                let url = api_url.join("repository/branches/")?.join(&branch)?;
                let request = create_get_request(&url)?;
                let response = self.client.request(request)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Rev(commit) => Ok(commit),
        }
    }

    fn download_manifest(&self, url: InternedUrl, commit: StrId) -> QuackResult<Manifest> {
        // Docs: `https://docs.gitlab.com/api/repository_files/#retrieve-a-raw-file-from-a-repository`.
        let mut url = url.join(&format!(
            "repository/files/{}/raw",
            PackageLoader::MANIFEST_NAME
        ))?;
        url.set_query(Some(&format!("ref={}", commit)));
        let request = create_get_request(&url)?;
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

#[derive(Deserialize)]
/// Type representing the interesting part of the response to `/repository/branches/<branch>` and `/repository/tags/<tag>` requests.
/// Based on `https://docs.gitlab.com/api/branches/#retrieve-a-repository-branch` and
/// `https://docs.gitlab.com/api/tags/#retrieve-a-single-repository-tag`.
struct CommitResponse {
    pub commit: CommitResponseId,
}

/// Helper for [`CommitResponse`].
#[derive(Deserialize)]
struct CommitResponseId {
    pub id: String,
}

/// Deserialize the response for requests `.../branches/<branch>` and `.../tags/<tag>` and get the `commit.id` field.
fn get_commit_from_response(response: Response) -> QuackResult<StrId> {
    let data: CommitResponse = response
        .deserialize_json()
        .context("failed to deserialize response")?;
    Ok(data.commit.id.into())
}

#[derive(Deserialize)]
/// Type representing the interesting part of the response to get repo request.
/// Based on `https://docs.gitlab.com/api/projects/#retrieve-a-project`.
struct DefaultBranchResponse {
    default_branch: String,
}

/// Deserialize the response for request to the url of the repository and get the `default_branch` field.
fn get_default_branch_from_response(response: Response) -> QuackResult<StrId> {
    let data: DefaultBranchResponse = response
        .deserialize_json()
        .context("failed to deserialize response")?;
    Ok(data.default_branch.into())
}

fn create_get_request(url: &Url) -> QuackResult<Request> {
    let mut request = create_http_request(url, http::Method::GET, vec![])?;
    request
        .headers_mut()
        .entry(header::PRAGMA)
        .or_insert(HeaderValue::from_static(defaults::PRAGMA_HEADER_WITH_VALUE));
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
    use crate::quackpack::core::fetcher::git::gitlab_client::GitlabClient;
    use crate::quackpack::core::fetcher::git::pseudo_git_client::PseudoGitClient;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn base_api_url() {
        let repo_url = "https://gitlab.com/foo/xd".to_url().unwrap().into();
        let api_url = "https://gitlab.com/api/v4/projects/foo%2Fxd/"
            .to_url()
            .unwrap();
        assert_eq!(api_url, GitlabClient::get_api_url(repo_url).unwrap())
    }
}
