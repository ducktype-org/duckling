//! Ducknest registry communication.
use std::io::Write;
use std::path::Path;
use std::sync::Arc;

use endpoints::UrlExt;
use http::{HeaderValue, header};
use tracing::{debug, info};
use url::Url;

use super::http_async::AsyncHttpClient;
use super::types;
use super::util::http::Request;
use super::util::http::traits_extensions::ResponseExt;
use crate::quackpack::core::fetcher::util::http::defaults;
use crate::quackpack::schemas::registry;
use crate::util::file_locks::LockedFile;
use crate::{QuackResult, QuackResultContext, StrId, qp_bail_internal};

mod endpoints;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// General client communicating with a registry instance over HTTP.
pub struct DucknestClient<'duck> {
    client: Arc<AsyncHttpClient<'duck>>,
}

impl<'duck> DucknestClient<'duck> {
    /// Construct a new [`DucknestClient`].
    pub fn new(client: Arc<AsyncHttpClient<'duck>>) -> Self {
        Self { client }
    }

    /// Retrieve metadata for a specific package from a Ducknest instance.
    #[tracing::instrument(skip(self))]
    pub async fn get_exact_metadata(
        &self,
        package: &types::PackageWithUrl,
    ) -> QuackResult<registry::Manifest> {
        debug!("fetching");
        let url = package.url.for_exact_metadata(&package.into())?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request).await?;
        let data = response.deserialize_json()?;
        info!("fetched");
        Ok(data)
    }

    /// Retrieve all metadata for a specific package from a Ducknest instance.
    #[tracing::instrument(skip(self, url), fields(url = url.as_str()))]
    pub async fn get_multi_metadata(
        &self,
        url: &Url,
        package: StrId,
    ) -> QuackResult<types::MultiMetadata> {
        debug!("fetching");
        let url = url.for_multi_metadata(package.as_str())?;

        let request = create_get_request(&url)?;

        let response = self.client.request(request).await?;
        let data = response.deserialize_json()?;
        info!("fetched");
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
        debug!("publishing");
        qp_bail_internal!("publishing is not yet implemented")
    }

    /// Download a package blob from a Ducknest instance and save it to a file.
    #[tracing::instrument(skip(self))]
    pub async fn fetch_blob(
        &self,
        package: &types::PackageWithUrl,
        mut target: LockedFile,
    ) -> QuackResult<()> {
        debug!("fetching");
        let url = package.url.for_blob(&package.into())?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request).await?;

        info!("fetched");
        debug!("saving response to file");
        target
            .write_all(response.body())
            .with_context(|| format!("failed to write to `{}`", target.path().display()))?;
        target
            .flush()
            .with_context(|| format!("failed to flush `{}`", target.path().display()))?;
        info!("saved to file");
        Ok(())
    }

    /// Search the Ducknest instance for all packages that match the provided query.
    #[tracing::instrument(skip(self))]
    pub async fn search(&self, url: &Url, query: &str) -> QuackResult<types::SearchResult> {
        debug!("searching");
        let url = url.for_search(query)?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request).await?;
        let data = response.deserialize_json()?;
        info!("got search response");
        Ok(data)
    }
}

fn create_get_request(url: &Url) -> QuackResult<Request> {
    let mut request = create_http_request(url, http::Method::GET, vec![])?;
    request
        .headers_mut()
        .entry(header::PRAGMA)
        .or_insert(HeaderValue::from_static(defaults::PRAGMA_HEADER_WITH_VALUE));
    request
        .headers_mut()
        .entry(header::EXPECT)
        .or_insert(HeaderValue::from_static(defaults::EXPECT_HEADER_WITH_VALUE));
    Ok(request)
}

fn create_http_request(url: &Url, method: http::Method, body: Vec<u8>) -> QuackResult<Request> {
    debug!(%method, %url, ?body, "building a request");
    http::Request::builder()
        .uri(url.as_str())
        .method(method)
        .version(http::Version::HTTP_2)
        .body(body)
        .context_internal("failed to build an HTTP request")
}
