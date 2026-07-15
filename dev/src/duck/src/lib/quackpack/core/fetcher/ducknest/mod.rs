//! Ducknest registry communication.
use std::io::Write;
use std::path::Path;

use endpoints::UrlExt;
use tracing::debug;
use url::Url;

use super::http::HttpClient;
use super::types;
use super::util::http::Request;
use super::util::http::traits_extensions::ResponseExt;
use crate::quackpack::schemas::registry;
use crate::util::file_locks::LockedFile;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail_internal};

mod endpoints;

#[cfg(test)]
mod tests;

#[derive(Debug, Clone)]
/// General client communicating with a registry instance over HTTP.
pub struct DucknestClient<'duck> {
    client: HttpClient<'duck>,
}

impl<'duck> DucknestClient<'duck> {
    /// Construct a new [`DucknestClient`].
    pub fn new(ctx: &'duck DuckContext) -> Self {
        Self {
            client: HttpClient::new(ctx),
        }
    }

    /// Retrieve metadata for a specific package from a Ducknest instance.
    #[tracing::instrument(skip(self))]
    pub fn get_exact_metadata(
        &self,
        package: &types::PackageWithUrl,
    ) -> QuackResult<registry::Manifest> {
        debug!("fetching...");
        let url = package.url.for_exact_metadata(&package.into())?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request)?;
        let data = response.deserialize_json()?;
        Ok(data)
    }

    /// Retrieve all metadata for a specific package from a Ducknest instance.
    #[tracing::instrument(skip(self, url), fields(url = url.as_str()))]
    pub fn get_multi_metadata(
        &self,
        url: &Url,
        package: StrId,
    ) -> QuackResult<types::MultiMetadata> {
        debug!("fetching...");
        let url = url.for_multi_metadata(package.as_str())?;

        let request = create_get_request(&url)?;

        let response = self.client.request(request)?;
        let data = response.deserialize_json()?;
        Ok(data)
    }

    /// Publish a package to a Ducknest instance.
    #[tracing::instrument(skip(self, url), fields(url = url.as_str()))]
    pub fn publish_package(
        &self,
        url: &Url,
        schema: &registry::Manifest,
        path: &Path,
    ) -> QuackResult<()> {
        debug!("publishing...");
        qp_bail_internal!("publishing is not yet implemented")
    }

    /// Download a package blob from a Ducknest instance and save it to a file.
    #[tracing::instrument(skip(self))]
    pub fn fetch_blob(
        &self,
        package: &types::PackageWithUrl,
        mut target: LockedFile,
    ) -> QuackResult<()> {
        debug!("fetching...");
        let url = package.url.for_blob(&package.into())?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request)?;

        target
            .write_all(response.body())
            .with_context(|| format!("failed to write to `{}`", target.path().display()))?;
        target
            .flush()
            .with_context(|| format!("failed to flush `{}`", target.path().display()))?;
        Ok(())
    }

    /// Search the Ducknest instance for all packages that match the provided query.
    #[tracing::instrument(skip(self))]
    pub fn search(&self, url: &Url, query: &str) -> QuackResult<types::SearchResult> {
        debug!("searching...");
        let url = url.for_search(query)?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request)?;
        let data = response.deserialize_json()?;
        Ok(data)
    }
}

fn create_get_request(url: &Url) -> QuackResult<Request> {
    create_http_request(url, http::Method::GET, vec![])
}

fn create_http_request(url: &Url, method: http::Method, body: Vec<u8>) -> QuackResult<Request> {
    debug!(%method, %url, "making an `{method}` request for `{url}`");
    http::Request::builder()
        .uri(url.as_str())
        .method(method)
        .body(body)
        .context_internal("failed to build an HTTP request")
}
