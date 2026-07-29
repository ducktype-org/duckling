use http::header;
use tracing::debug;
use url::Url;

use crate::quackpack::core::fetcher::http::HttpClient;
use crate::quackpack::core::fetcher::util::http::{Request, Response, defaults};
use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

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

    /// Docs: <https://docs.gitlab.com/api/projects/#retrieve-a-project>.
    /// Get general information about the project, used to get the default branch name.
    pub fn retrieve_project(&self, repo_api_url: &Url) -> QuackResult<Response> {
        self.request(repo_api_url)
    }

    /// Docs: <https://docs.gitlab.com/api/commits/#retrieve-a-commit>.
    /// Get information about a commit specified by branch, tag or 1-byte commit id.
    /// Used to get the full commit identifier.
    pub fn retrieve_a_commit(&self, repo_api_url: &Url, reference: StrId) -> QuackResult<Response> {
        let url = repo_api_url.join("repository/commits/")?.join(&reference)?;
        self.request(&url)
    }

    /// Docs: <https://docs.gitlab.com/api/repository_files/#retrieve-a-raw-file-from-a-repository>.
    /// Download a raw file from the repository at specific commit.
    /// Used to download the manifest.
    pub fn download_file_from_commit(
        &self,
        repo_api_url: &Url,
        path_to_file: &str,
        commit: StrId,
    ) -> QuackResult<Response> {
        let mut url = repo_api_url
            .join("repository/files/")?
            .join(path_to_file)?
            .join("raw/")?;
        url.set_query(Some(&format!("ref={}", commit)));
        self.request(&url)
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
