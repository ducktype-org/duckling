use std::path::{Path, PathBuf};

use rustvil::fs::{MkdirOptions, PathExt};
use tracing::{Level, debug, span};
use url::Url;

use crate::{
    DuckCtx, QpCtx, QuackResult, QuackResultContext, StrId, qp_bail_internal,
    quackpack::{
        core::{self, Git},
        schemas::registry,
    },
};

pub mod cache;
pub mod ducknest;
pub mod git;
pub mod types;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// A class for managing HTTP and Git clients, and caching metadata.
pub struct Fetcher<'duck> {
    ctx: &'duck DuckCtx,
    ducknest_client: ducknest::DucknestClient,
    #[allow(unused)] // @TODO: #1737 Remove this
    git_client: git::GitClient,
    cache: cache::ManifestCache,
    download_cache_path: &'duck Path,
    #[allow(unused)] // @TODO: #1905 Remove this
    artifacts_cache_path: &'duck Path,
}

impl<'duck> Fetcher<'duck> {
    const DEFAULT_BLOB_FILENAME: &'static str = "source.tar.gz";

    /// Create a new [`Fetcher`].
    ///
    /// # Errors
    /// Errors can occur in a few situations:
    /// 1. failed to create any of the internal files,
    /// 2. failed to initialize any of the underlying clients,
    /// 3. failed to initialize cache manager.
    pub fn new(ctx: &'duck DuckCtx) -> QuackResult<Self> {
        let metadata_path = ctx.duck_home().ensure_metadata_db()?;
        let artifacts_cache_path = ctx.duck_home().ensure_artifacts_dir()?;
        let download_cache_path = ctx.duck_home().ensure_downloads_dir()?;
        debug!(
            "metadata is at `{}`, artifacts are at `{}`, and downloads are at `{}`",
            metadata_path.display(),
            artifacts_cache_path.display(),
            download_cache_path.display()
        );
        let ducknest_client = ducknest::DucknestClient::new()?;
        let cache = cache::ManifestCache::new(cache::CacheLocation::Path(metadata_path))?;
        let git_client = git::GitClient {};
        Ok(Self {
            ctx,
            ducknest_client,
            git_client,
            cache,
            artifacts_cache_path,
            download_cache_path,
        })
    }

    /// Retrieve metadata for `package` from a given Ducknest instance.
    ///
    /// Exact cache hit takes precedence over HTTP requests.
    pub async fn get_package_metadata(
        &self,
        package: &types::PackageWithUrl,
    ) -> QuackResult<registry::Manifest> {
        let span = span!(Level::DEBUG, "metadata", package = ?package);
        let _guard = span.enter();
        if let Some(cached) = self.cache.get_manifest(package).await? {
            debug!("cache hit");
            return Ok(cached);
        }
        debug!("cache miss");
        let result = self.ducknest_client.get_exact_metadata(package).await?;
        self.cache
            .add_or_replace_manifest(package, result.clone())
            .await?;
        Ok(result)
    }

    /// Retrieve metadata for all versions of a `package_name` from a given Ducknest instance at
    /// `url`.
    ///
    /// This method does __not__ look up in the cache, however it saves all fetched metadata, so
    /// future calls to [`get_package_metadata`](Self::get_package_metadata) should cache hit.
    pub async fn get_package_all_metadata(
        &self,
        url: &Url,
        package_name: StrId,
    ) -> QuackResult<types::MultiMetadata> {
        let span = span!(Level::DEBUG, "all metadata", package = %package_name, url = %url);
        let _guard = span.enter();
        let result = self
            .ducknest_client
            .get_multi_metadata(url, package_name)
            .await?;
        self.cache
            .add_or_replace_multiple_manifests(url.clone(), result.packages_metadata.clone())
            .await?;
        Ok(result)
    }

    /// Fetch a source of a `package`. Returns a path to the file where the blob has been saved.
    pub async fn fetch_package_blob(
        &self,
        package: &types::PackageWithUrl,
    ) -> QuackResult<PathBuf> {
        let span = span!(Level::DEBUG, "blob", package = ?package);
        let _guard = span.enter();
        let destination = self
            .download_cache_path
            .join(package.id)
            .join(package.version.to_string())
            .join(Self::DEFAULT_BLOB_FILENAME);

        if destination.exists() {
            return Ok(destination);
        }

        if let Some(parent) = destination.parent() {
            parent
                .mkdir(MkdirOptions::WithParents)
                .with_context(|| format!("failed to create directory `{}`", parent.display()))?;
        } else {
            qp_bail_internal!("path without a parent")
        }
        self.ducknest_client
            .fetch_blob(package, &destination)
            .await?;
        Ok(destination)
    }

    /// Clone a git repository pointed by `source` to the `destination_directory`.
    pub async fn clone_from_git(
        &self,
        source: &Git,
        destination_directory: &std::path::Path,
    ) -> QuackResult<types::GitCloneResponse> {
        let span = span!(Level::DEBUG, "git clone", source = ?source, to = %destination_directory.display());
        let _guard = span.enter();
        git::GitClient::clone_async(source, destination_directory, &QpCtx::new(self.ctx)).await
    }

    /// Same as [`clone_from_git`](Self::clone_from_git), but target directory is a temporary
    /// directory, and only a [`Package`](core::Package) is returned.
    pub async fn get_git_metadata(&self, source: &Git) -> QuackResult<core::Package> {
        let dir = tempfile::tempdir().context("failed to create a temporary directory")?;
        let result = self.clone_from_git(source, dir.path()).await?;
        Ok(result.package)
    }
}
