//! Ducknest registry communication.
use std::path::Path;

use endpoints::UrlExt;
use tracing::debug;
use url::Url;

use super::http::HttpClient;
use crate::quackpack::core::fetcher::types;
use crate::quackpack::schemas::registry;
use crate::{DuckContext, QuackResult, StrId, qp_bail_internal};

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

        let response = self.client.get(&url)?;
        response.deserialize_json()
    }

    /// Retrieve all metadata for a specific package from a Ducknest instance.
    #[tracing::instrument(skip(self, url), fields(url = url.as_str()))]
    pub fn get_multi_metadata(
        &self,
        url: &Url,
        package: StrId,
    ) -> QuackResult<types::MultiMetadata> {
        debug!("fetching...");
        let req_url = url.for_multi_metadata(package)?;

        let response = self.client.get(&req_url)?;
        response.deserialize_json()
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
    pub fn fetch_blob(&self, package: &types::PackageWithUrl, target: &Path) -> QuackResult<()> {
        debug!("fetching...");
        let url = package.url.for_blob(&package.into())?;
        self.client.get_to_file(&url, target)
    }

    /// Search the Ducknest instance for all packages that match the provided query.
    #[tracing::instrument(skip(self))]
    pub fn search(&self, url: &Url, query: &str) -> QuackResult<types::SearchResult> {
        debug!("searching...");
        let req_url = url.for_search(query)?;

        let response = self.client.get(&req_url)?;
        response.deserialize_json()
    }
}
