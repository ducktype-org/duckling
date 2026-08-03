//! Module for using parts of Gitlab's API.
//! The whole documentation can be found here: <https://docs.gitlab.com/api/api_resources/>.
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
use http::header;
use percent_encoding::{AsciiSet, CONTROLS, utf8_percent_encode};
use serde::Deserialize;
use tracing::debug;
use url::Url;

use crate::quackpack::core::fetcher::http::HttpClient;
use crate::quackpack::core::fetcher::util::http::traits_extensions::ResponseExt;
use crate::quackpack::core::fetcher::util::http::{Request, Response, defaults};
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
pub struct GitlabApiClient<'duck> {
    client: HttpClient<'duck>,
}

impl<'duck> GitlabApiClient<'duck> {
    /// Creates a new [`GitlabApiClient`].
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
        // Check that this is a gitlab repo.
        if !domain.starts_with("gitlab") {
            return None;
        }
        // `path()` adds `/` in the beginning.
        let path: String = repo_url.path().chars().skip(1).collect();
        // Remove any `.git` in the end of the path.
        let path_trimmed = if let Some(path) = path.strip_suffix(".git") {
            path
        } else {
            &path
        };
        let path_encoded = utf8_percent_encode(path_trimmed, PATH_ENCODE_SET);
        let mut repo_api_url = format!("{scheme}://{domain}/api/v4/projects/{path_encoded}/")
            .to_url()
            .ok()?;
        repo_api_url.set_username(user).ok()?;
        repo_api_url.set_port(port).ok()?;
        Some(repo_api_url)
    }

    /// Docs: <https://docs.gitlab.com/api/projects/#retrieve-a-project>.
    /// Get general information about the project, used to get the default branch name.
    pub fn retrieve_project(&self, repo_api_url: &Url) -> QuackResult<StrId> {
        let response = self.request(repo_api_url)?;
        get_default_branch_from_response(response)
    }

    /// Docs: <https://docs.gitlab.com/api/commits/#retrieve-a-commit>.
    /// Get information about a commit specified by branch, tag or 1-byte commit id.
    /// Used to get the full commit identifier.
    pub fn retrieve_a_commit(&self, repo_api_url: &Url, reference: StrId) -> QuackResult<StrId> {
        let url = repo_api_url.join("repository/commits/")?.join(&reference)?;
        let response = self.request(&url)?;
        get_commit_from_response(response)
    }

    /// Docs: <https://docs.gitlab.com/api/repository_files/#retrieve-a-raw-file-from-a-repository>.
    /// Download a raw file from the repository at specific commit.
    /// Used to download the manifest.
    pub fn download_file_from_commit(
        &self,
        repo_api_url: &Url,
        path_to_file: &str,
        commit: StrId,
    ) -> QuackResult<String> {
        let path_encoded = utf8_percent_encode(path_to_file, PATH_ENCODE_SET);
        let mut url = repo_api_url
            .join("repository/files/")?
            .join(&path_encoded.to_string())?
            .join("/raw")?;
        url.set_query(Some(&format!("ref={}", commit)));
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

#[derive(Deserialize)]
/// Type representing the interesting part of the response to `/repository/commits/reference`.
/// Based on <https://docs.gitlab.com/api/commits/#retrieve-a-commit>.
struct CommitResponse {
    id: String,
}

/// Deserialize the response for requests `/repository/commits/reference` and get the `id` field.
fn get_commit_from_response(response: Response) -> QuackResult<StrId> {
    let data: CommitResponse = response
        .deserialize_json()
        .context("failed to deserialize response")?;
    Ok(data.id.into())
}

#[derive(Deserialize)]
/// Type representing the interesting part of the response to get repo request.
/// Based on <https://docs.gitlab.com/api/projects/#retrieve-a-project>.
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

#[cfg(test)]
mod test {
    use crate::quackpack::core::fetcher::git::fast_path::gitlab_api_client::GitlabApiClient;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn base_api_url() {
        let repo_url = "https://gitlab.com/foo/xd".to_url().unwrap();
        let api_url = "https://gitlab.com/api/v4/projects/foo%2Fxd/"
            .to_url()
            .unwrap();
        assert_eq!(api_url, GitlabApiClient::get_api_url(&repo_url).unwrap())
    }

    #[test]
    fn remove_dot_git() {
        let repo_url = "https://gitlab.com/foo/xd.git".to_url().unwrap();
        let api_url = "https://gitlab.com/api/v4/projects/foo%2Fxd/"
            .to_url()
            .unwrap();
        assert_eq!(api_url, GitlabApiClient::get_api_url(&repo_url).unwrap())
    }
}
