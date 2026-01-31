use std::path::Path;

use reqwest::multipart::Form;
use reqwest::{Client, ClientBuilder, redirect::Policy};
use tokio::io::AsyncWriteExt;
use tracing::debug;
use url::Url;

use crate::StrId;
use crate::quackpack::core::fetcher::ducknest::endpoints::UrlExt;
use crate::{QuackResult, QuackResultContext, quackpack::core::fetcher::types};

use crate::quackpack::schemas::registry;

mod defaults;
mod endpoints;

#[cfg(test)]
mod tests;

#[derive(Debug, Clone)]
pub struct DucknestClient {
    client: Client,
}

impl DucknestClient {
    /// Construct a new [`DucknestClient`].
    ///
    /// # Errors
    /// This method can fail if an underlying backend can't configure a TLS.
    pub fn new() -> QuackResult<Self> {
        let default_headers = [
            (reqwest::header::EXPECT, defaults::EXPECT),
            (reqwest::header::PRAGMA, defaults::PRAGMA),
        ]
        .into_iter()
        .collect();

        let client = ClientBuilder::new()
            .deflate(true)
            .gzip(true)
            .default_headers(default_headers)
            .user_agent(defaults::DUCK_USER_AGENT)
            .connect_timeout(defaults::CONNECT_TIMEOUT)
            .read_timeout(defaults::REQUEST_TIMEOUT)
            .redirect(Policy::limited(defaults::MAX_REDIRECTS))
            .build()
            .context_internal("failed to initialize TLS backend")?;

        Ok(Self { client })
    }

    /// Retrieve metadata for a specific package from a Ducknest instance.
    pub async fn get_exact_metadata(
        &self,
        package: &types::PackageWithUrl,
    ) -> QuackResult<registry::Manifest> {
        debug!("fetching {package:?}");
        let url = package.url.for_exact_metadata(&package.into())?;

        self.client
            .get(url)
            .send()
            .await
            .with_context(|| {
                format!(
                    "while getting a metadata of `{}`@{} from `{}`",
                    package.id, package.version, package.url
                )
            })?
            .error_for_status()
            .with_context(|| {
                format!(
                    "while getting a metadata of `{}`@{} from `{}`",
                    package.id, package.version, package.url
                )
            })?
            .json::<registry::Manifest>()
            .await
            .context("registry hasn't responded with appropriate JSON schema")
    }

    /// Retrieve all metadata for a specific package from a Ducknest instance.
    pub async fn get_multi_metadata(
        &self,
        url: &Url,
        package: StrId,
    ) -> QuackResult<types::MultiMetadata> {
        debug!("fetching all metadata of `{package}` from `{url}`");
        let req_url = url.for_multi_metadata(package)?;

        self.client
            .get(req_url)
            .send()
            .await
            .with_context(|| format!("while getting a multi metadata of `{package}` from `{url}`"))?
            .error_for_status()
            .with_context(|| format!("while getting a multi metadata of `{package}` from `{url}`"))?
            .json::<types::MultiMetadata>()
            .await
            .context("registry hasn't responded with appropriate JSON schema")
    }

    /// Publish a package to a Ducknest instance.
    pub async fn publish_package(
        &self,
        url: &Url,
        schema: &registry::Manifest,
        path: &Path,
    ) -> QuackResult<()> {
        debug!(
            "publishing package `{}`@{} to `{url}`",
            schema.metadata.name,
            path.display()
        );
        let json = serde_json::to_vec(schema).context_internal("schema not being a JSON?")?;
        let create_url = url.for_new_package()?;
        // Create a new package at the registry.
        self.client
            .post(create_url)
            .body(json)
            .header(reqwest::header::CONTENT_TYPE, defaults::APPLICATION_JSON)
            .send()
            .await
            .with_context(|| {
                format!(
                    "while creating a new package `{}` at `{url}`",
                    schema.metadata.name
                )
            })?
            .error_for_status_ref()?;

        let blob_url = url.for_new_blob(&schema.into())?;
        let form = Form::new()
            .file("package_source", path)
            .await
            .with_context(|| {
                format!(
                    "while trying to read from `{}` in order to get a PUT body",
                    path.display()
                )
            })?;
        self.client
            .put(blob_url)
            .multipart(form)
            .send()
            .await
            .with_context(|| {
                format!(
                    "while sending a source of a package `{}`@{} to the `{}`",
                    schema.metadata.name, schema.metadata.version, url
                )
            })?
            .error_for_status_ref()?;
        Ok(())
    }

    /// Download a package blob from a Ducknest instance and save it to a file.
    pub async fn fetch_blob(
        &self,
        package: &types::PackageWithUrl,
        target: &Path,
    ) -> QuackResult<()> {
        debug!("fetching a blob of `{package:?}` to `{}`", target.display());
        let mut file = tokio::fs::File::create(target).await.with_context(|| {
            format!(
                "while creating a file at `{}` in order to download a blob of `{}`@{} from `{}`",
                target.display(),
                package.id,
                package.version,
                package.url
            )
        })?;
        let url = package.url.for_blob(&package.into())?;
        let mut response = self.client.get(url).send().await.with_context(|| {
            format!(
                "while getting a package `{}`@{} blob from `{}`",
                package.id, package.version, package.url
            )
        })?;
        response.error_for_status_ref()?;
        while let Some(chunk) = response.chunk().await.with_context(|| {
            format!(
                "while getting a blob chunk of `{}`@{} from `{}`",
                package.id, package.version, package.url
            )
        })? {
            file.write_all(&chunk).await.with_context(|| {
                format!(
                    "while writing a chunk of a `{}`@{} from `{}` to `{}`",
                    package.id,
                    package.version,
                    package.url,
                    target.display(),
                )
            })?;
        }
        file.flush().await.with_context(|| {
            format!(
                "while flushing contents of a `{}`@{} to `{}`",
                package.id,
                package.version,
                target.display()
            )
        })?;
        Ok(())
    }

    /// Search the Ducknest instance for all packages that match the provided query.
    pub async fn search(&self, url: &Url, query: &str) -> QuackResult<types::SearchResult> {
        debug!("searching `{query}` on `{url}`");
        let req_url = url.for_search(query)?;

        self.client
            .get(req_url)
            .send()
            .await
            .with_context(|| format!("while searching `{query}` on `{url}`"))?
            .error_for_status()
            .with_context(|| format!("while searching `{query}` on `{url}`"))?
            .json::<types::SearchResult>()
            .await
            .context("registry hasn't responded with appropriate JSON schema")
    }
}
