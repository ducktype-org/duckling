#![expect(dead_code)]
use std::path::Path;

use http::{HeaderValue, header};
use serde_json::Value;
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
        let scheme = repo_url.scheme();
        let domain = repo_url.domain()?;
        // Check that this is a github repo.
        if !domain.starts_with("github") {
            return None;
        }
        let path = repo_url.path();
        let Ok(api_url) = format!("{scheme}://api.{domain}/repos{path}/").to_url() else {
            return None;
        };
        Some(api_url.into())
    }

    fn get_commit_hash(&self, api_url: InternedUrl, reference: GitReference) -> QuackResult<StrId> {
        match reference {
            GitReference::Default => {
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
                let url = api_url.join("git/ref/tags/")?.join(&tag)?;
                let request = create_get_request(&url)?;
                let response = self.client.request(request)?;
                let commit_hash = get_commit_from_response(response)?;
                Ok(commit_hash)
            }
            GitReference::Branch(branch) => {
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

/// Deserialize the response for requests `.../branches/<branch>` and `.../tags/<tag>` and get the `commit.id` field.
fn get_commit_from_response(response: Response) -> QuackResult<StrId> {
    let data: Value = response.deserialize_json()?;
    let commit_hash = data
        .get("object")
        .and_then(|v| v.get("sha"))
        .context("failed to find the commit hash in the response")?;
    let commit_hash = commit_hash
        .as_str()
        .context("failed to interpret the commit hash as string")?
        .into();
    Ok(commit_hash)
}

/// Deserialize the response for request to the url of the repository and get the `default_branch` field.
fn get_default_branch_from_response(response: Response) -> QuackResult<StrId> {
    let data: Value = response.deserialize_json()?;
    let default_branch = data
        .get("default_branch")
        .context("failed to find the default branch in the response")?;
    let default_branch = default_branch
        .as_str()
        .context("failed to interpret the default branch as string")?
        .into();
    Ok(default_branch)
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
