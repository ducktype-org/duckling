// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Ducknest registry communication.
use std::io::Write;
use std::path::Path;
use std::sync::Arc;

use endpoints::UrlExt;
use tracing::{debug, trace};
use url::Url;

use super::http_async::AsyncHttpClient;
use super::types;
use super::util::http::Request;
use super::util::http::traits_extensions::ResponseExt;
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
    #[tracing::instrument(level = "debug", skip(self))]
    pub async fn get_exact_metadata(
        &self,
        package: types::PackageWithUrl,
    ) -> QuackResult<registry::Manifest> {
        let url = package.url.for_exact_metadata(package.into())?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request).await?;
        let data = response.deserialize_json()?;
        trace!("fetched");
        Ok(data)
    }

    /// Retrieve all metadata for a specific package from a Ducknest instance.
    #[tracing::instrument(level = "debug", skip(self, url), fields(%url))]
    pub async fn get_multi_metadata(
        &self,
        url: &Url,
        package: StrId,
    ) -> QuackResult<types::MultiMetadata> {
        let url = url.for_multi_metadata(package.as_str())?;

        let request = create_get_request(&url)?;

        let response = self.client.request(request).await?;
        let data = response.deserialize_json()?;
        trace!("fetched");
        Ok(data)
    }

    /// Publish a package to a Ducknest instance.
    #[tracing::instrument(level = "debug", skip(self, url), fields(%url))]
    pub fn publish_package(
        &self,
        url: &Url,
        schema: &registry::Manifest,
        path: &Path,
    ) -> QuackResult<()> {
        qp_bail_internal!("publishing is not yet implemented")
    }

    /// Download a package blob from a Ducknest instance and save it to a file.
    #[tracing::instrument(level = "debug", skip(self))]
    pub async fn fetch_blob(
        &self,
        package: types::PackageWithUrl,
        mut target: &LockedFile,
    ) -> QuackResult<()> {
        let url = package.url.for_blob(package.into())?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request).await?;

        debug!(
            "fetched {} bytes, saving them to `{}`",
            response.body().len(),
            target.path().display()
        );
        target
            .write_all(response.body())
            .with_context(|| format!("failed to write to `{}`", target.path().display()))?;
        target
            .flush()
            .with_context(|| format!("failed to flush `{}`", target.path().display()))?;
        Ok(())
    }

    /// Search the Ducknest instance for all packages that match the provided query.
    #[tracing::instrument(level = "debug", skip(self))]
    pub async fn search(&self, url: &Url, query: &str) -> QuackResult<types::SearchResult> {
        let url = url.for_search(query)?;
        let request = create_get_request(&url)?;

        let response = self.client.request(request).await?;
        let data = response.deserialize_json()?;
        trace!("got search response");
        Ok(data)
    }
}

fn create_get_request(url: &Url) -> QuackResult<Request> {
    create_http_request(url, http::Method::GET, vec![])
}

fn create_http_request(url: &Url, method: http::Method, body: Vec<u8>) -> QuackResult<Request> {
    debug!("building a `{method}` request to `{url}`");
    trace!(?body);
    http::Request::builder()
        .uri(url.as_str())
        .method(&method)
        .version(http::Version::HTTP_2)
        .body(body)
        .with_context_internal(|| {
            format!("failed to build an HTTP request: url: `{url}`, method: `{method:#?}`")
        })
}
