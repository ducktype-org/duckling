use http::{HeaderName, HeaderValue, header};
use tracing::debug;
use url::Url;

use crate::quackpack::core::fetcher::http::HttpClient;
use crate::quackpack::core::fetcher::util::http::{Request, Response, defaults};
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

    /// Docs: <https://docs.github.com/en/rest/commits/commits?apiVersion=2026-03-10#list-commits>.
    /// Get list of information about commits, starting from the default branch (if `reference` is [`None`])
    /// or the commit specified by branch, tag or 1-byte commit id.
    /// Used to get the full commit identifier.
    pub fn retrieve_a_commit(
        &self,
        repo_api_url: &Url,
        reference: Option<StrId>,
    ) -> QuackResult<Response> {
        let mut url = repo_api_url.join("commits")?;
        if let Some(reference) = reference {
            url.set_query(Some(&format!("sha={reference}")));
        }
        self.request(&url)
    }

    /// Docs: <https://docs.github.com/en/rest/repos/contents?apiVersion=2026-03-10#get-repository-content>.
    /// Download a raw file from the repository at specific commit.
    /// Used to download the manifest.
    pub fn download_file_from_commit(
        &self,
        repo_api_url: &Url,
        path_to_file: &str,
        commit: StrId,
    ) -> QuackResult<Response> {
        let mut url = repo_api_url.join("contents/")?.join(path_to_file)?;
        url.set_query(Some(&format!("ref={commit}")));
        self.request(&url)
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
        debug!(%method, %url, "making an `{method}` request for `{url}`");
        http::Request::builder()
            .uri(url.as_str())
            .method(method)
            .body(body)
            .context_internal("failed to build an HTTP request")
    }

    /// Get the underlying [`DuckContext`].
    pub fn ctx(&self) -> &DuckContext {
        self.client.ctx()
    }
}
