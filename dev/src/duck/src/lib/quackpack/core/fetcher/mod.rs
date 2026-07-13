//! [`Fetcher`] manages all network-related clients.
//!
//! It incorporates [`DucknestClient`](ducknest::DucknestClient) with
//! [`ManifestCache`](cache::ManifestCache), and provides another layer of abstraction over the
//! [`GitClient`](git::GitClient).
use std::path::PathBuf;

use tempfile::TempDir;
use tracing::debug;
use url::Url;

use crate::quackpack::core::GitReference;
use crate::quackpack::core::fetcher::types::{FetcherResponse, PackageWithUrl};
use crate::quackpack::schemas::registry;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::util::file_locks::FileLockManager;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

pub mod cache;
pub mod ducknest;
pub mod git;
pub mod http;
pub mod types;
pub mod util;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// A class for managing HTTP and Git clients, and caching metadata.
pub struct Fetcher<'duck> {
    ctx: &'duck DuckContext,
    ducknest_client: ducknest::DucknestClient<'duck>,
    #[allow(unused)] // @TODO: #1737 Remove this
    git_client: git::GitClient,
    cache: cache::ManifestCache,
    download_cache_path: FileLockManager,
    #[allow(unused)] // @TODO: #1905 Remove this
    artifacts_cache_path: FileLockManager,
}

impl<'duck> Fetcher<'duck> {
    /// Filename of the default package's compressed source.
    const DEFAULT_BLOB_FILENAME: &'static str = "source.tar.gz";
    /// URL of the default Ducknest instance.
    pub const DEFAULT_REGISTRY_URL: &'static str = "http://localhost:9001";

    /// Create a new [`Fetcher`].
    ///
    /// # Errors
    /// Errors can occur in a few situations:
    /// 1. failed to create any of the internal files,
    /// 2. failed to initialize any of the underlying clients,
    /// 3. failed to initialize cache manager.
    pub fn new(ctx: &'duck DuckContext) -> QuackResult<Self> {
        let metadata_path = ctx.duck_home().get_metadata_db_path();
        let artifacts_cache_path = ctx.duck_home().artifacts();
        artifacts_cache_path.mkdir()?;
        let download_cache_path = ctx.duck_home().downloads();
        download_cache_path.mkdir()?;
        debug!(
            "metadata is at `{}`, artifacts are at `{}`, and downloads are at `{}`",
            metadata_path.display(),
            artifacts_cache_path.display(),
            download_cache_path.display()
        );
        let ducknest_client = ducknest::DucknestClient::new(ctx);
        let cache = cache::ManifestCache::new(cache::CacheLocation::Path(metadata_path.as_path()))?;
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
    #[tracing::instrument(skip(self))]
    pub fn get_package_metadata(
        &self,
        package: &types::PackageWithUrl,
    ) -> QuackResult<FetcherResponse<registry::Manifest>> {
        if let Some(cached) = self.cache.get_manifest(package)? {
            debug!("cache hit");
            return Ok(FetcherResponse::Some(cached));
        }
        debug!("cache miss");
        if self.ctx.is_offline() {
            return Ok(FetcherResponse::Offline);
        }
        let result = self
            .ducknest_client
            .get_exact_metadata(package)
            .with_context(|| {
                format!(
                    "while getting a metadata of `{}` version `{}`",
                    package.id, package.version
                )
            })?;
        self.cache
            .add_or_replace_manifest(package, result.clone())?;
        Ok(FetcherResponse::Some(result))
    }

    /// Retrieve metadata for all versions of a `package_name` from a given Ducknest instance at
    /// `url`.
    ///
    /// This method does only looks up manifests in the cache if `offline` is set to true.
    /// Otherwise __no__ lookup is performed.
    /// However, in that case it saves all fetched metadata, so
    /// future calls to [`get_package_metadata`](Self::get_package_metadata) should cache hit.
    #[tracing::instrument(skip(self))]
    pub fn get_package_all_metadata(
        &mut self,
        url: InternedUrl,
        package_name: StrId,
    ) -> QuackResult<FetcherResponse<types::MultiMetadata>> {
        if self.ctx.is_offline() {
            let package = PackageWithUrl {
                id: package_name,
                version: 1.into(),
                url,
            };
            let cached = self.cache.get_all_manifests(&package)?;
            return Ok(FetcherResponse::Some(types::MultiMetadata {
                packages_metadata: cached,
            }));
        }
        let result = self
            .ducknest_client
            .get_multi_metadata(&url, package_name)
            .with_context(|| format!("while getting a multimetadata of `{}`", package_name))?;
        self.cache
            .add_or_replace_multiple_manifests(url, result.packages_metadata.clone())?;
        Ok(FetcherResponse::Some(result))
    }

    /// Fetch a source of a `package`. Returns a path to the file where the blob has been saved.
    #[tracing::instrument(skip(self))]
    pub fn fetch_package_blob(&self, package: &types::PackageWithUrl) -> QuackResult<PathBuf> {
        let destination = self
            .download_cache_path
            .join(package.id)
            .join(package.version.to_string());

        let blob_path = destination
            .not_locked_path()
            .join(Self::DEFAULT_BLOB_FILENAME);

        if blob_path.exists() {
            debug!("cache hit");
            return Ok(blob_path);
        }

        let blob = destination.open_exclusive(Self::DEFAULT_BLOB_FILENAME, self.ctx)?;

        self.ducknest_client
            .fetch_blob(package, blob)
            .with_context(|| {
                format!(
                    "while downloading a source of `{}` version `{}`",
                    package.id, package.version
                )
            })?;
        Ok(blob_path)
    }

    /// Clone a git repository pointed by `source` to the `destination_directory`.
    #[tracing::instrument(skip(self))]
    pub fn clone_from_git_to_directory(
        &self,
        url: &Url,
        reference: GitReference,
        destination_directory: &std::path::Path,
    ) -> QuackResult<types::GitCloneResponse> {
        git::GitClient::clone_blocking(url, reference, destination_directory, self.ctx)
    }

    /// Same as [`clone_from_git_to_directory`](Self::clone_from_git_to_directory), but a target directory is a temporary
    /// directory.
    ///
    /// This is needed because storage paths depend on a commit, which we can only get after
    /// cloning a repository.
    #[tracing::instrument(skip(self, url) fields(url = url.as_str()))]
    pub fn clone_from_git(
        &self,
        url: &Url,
        reference: GitReference,
    ) -> QuackResult<(types::GitCloneResponse, TempDir)> {
        let dir = tempfile::tempdir().context("failed to create a temporary directory")?;
        let result = self.clone_from_git_to_directory(url, reference, dir.path())?;
        Ok((result, dir))
    }

    /// Get the [`DuckContext`] used to construct this [`Fetcher`] instance.
    pub fn ctx(&self) -> &DuckContext {
        self.ctx
    }
}
