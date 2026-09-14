//! [`Fetcher`] manages all network-related clients.
//!
//! It incorporates [`DucknestClient`](ducknest::DucknestClient) with
//! [`ManifestCache`](cache::ManifestCache), and provides another layer of abstraction over the
//! [`GitClient`](git::GitClient).
use std::cell::RefCell;
use std::path::PathBuf;
use std::sync::Arc;

use tempfile::TempDir;
use tracing::{debug, error, info};
use url::Url;

use crate::quackpack::core::GitReference;
use crate::quackpack::core::fetcher::http_async::AsyncHttpClient;
use crate::quackpack::core::fetcher::types::{FetcherResponse, PackageWithUrl};
use crate::quackpack::schemas::registry;
use crate::quackpack::util::guards;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail};

pub mod cache;
pub mod ducknest;
pub mod git;
pub mod http;
pub mod http_async;
pub mod types;
pub mod util;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// A class for managing HTTP and Git clients, and caching metadata.
pub struct Fetcher<'duck> {
    ctx: &'duck DuckContext,
    ducknest_client: ducknest::DucknestClient<'duck>,
    git_client: git::GitClient<'duck>,
    git_fastpath_client: git::fast_path::GitFastPathClient<'duck>,
    cache: RefCell<cache::ManifestCache>,
    download_cache_path: FileLockManager,
    locks_path: FileLockManager,
    #[allow(unused)] // @TODO: #1905 Remove this
    artifacts_cache_path: FileLockManager,
}

impl<'duck> Fetcher<'duck> {
    /// Filename of the default package's compressed source.
    const DEFAULT_BLOB_FILENAME: &'static str = "source.tar.gz";

    /// Filename of the per-package lock.
    const PER_PACKAGE_LOCK: &'static str = ".duck.package.lock";

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
        let locks = ctx.duck_home().cache().join("locks");
        locks.mkdir()?;
        debug!(
            metadata = %metadata_path.display(),
            artifacts = %artifacts_cache_path.display(),
            downloads = %download_cache_path.display(),
            locks = %locks.display()
        );
        let http_client = Arc::new(AsyncHttpClient::new(ctx));
        let ducknest_client = ducknest::DucknestClient::new(http_client.clone());
        let cache = cache::ManifestCache::new(cache::CacheLocation::Path(metadata_path.as_path()))?;
        let git_client = git::GitClient::new(ctx);
        let git_fastpath_client = git::fast_path::GitFastPathClient::new(http_client);
        Ok(Self {
            ctx,
            ducknest_client,
            git_client,
            git_fastpath_client,
            cache: RefCell::new(cache),
            artifacts_cache_path,
            download_cache_path,
            locks_path: locks,
        })
    }

    /// Retrieve metadata for `package` from a given Ducknest instance.
    ///
    /// Exact cache hit takes precedence over HTTP requests.
    #[tracing::instrument(skip(self))]
    pub async fn get_package_metadata(
        &self,
        package: types::PackageWithUrl,
    ) -> QuackResult<FetcherResponse<registry::Manifest>> {
        if let Some(cached) = self.cache.borrow().get_manifest(package)? {
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
            .await
            .with_context(|| {
                format!(
                    "while getting a metadata of `{}` version {}",
                    package.name, package.version
                )
            })?;
        self.cache
            .borrow_mut()
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
    pub async fn get_package_all_metadata(
        &self,
        url: InternedUrl,
        package_name: StrId,
    ) -> QuackResult<FetcherResponse<types::MultiMetadata>> {
        if self.ctx.is_offline() {
            let package = PackageWithUrl {
                name: package_name,
                version: 1.into(),
                url,
            };
            let cached = self.cache.borrow().get_all_manifests(package)?;
            return Ok(FetcherResponse::Some(types::MultiMetadata {
                packages_metadata: cached,
            }));
        }
        let result = self
            .ducknest_client
            .get_multi_metadata(&url, package_name)
            .await
            .with_context(|| format!("while getting a multimetadata of `{}`", package_name))?;
        self.cache
            .borrow_mut()
            .add_or_replace_multiple_manifests(url, result.packages_metadata.clone())?;
        Ok(FetcherResponse::Some(result))
    }

    /// Fetch a source of a `package`. Returns a path to the file where the blob has been saved.
    #[tracing::instrument(skip(self))]
    async fn fetch_package_blob(&self, package: types::PackageWithUrl) -> QuackResult<PathBuf> {
        // Firstly acquire a "global" per package lock.
        // Otherwise, under the same global fetcher lock, we can start two downloads of the same package,
        // and one can see existing but empty path.
        let _lock = self.acquire_package_lock(package.into())?;
        let destination = self
            .download_cache_path
            .join(package.name)
            .join(package.version.to_string());

        let blob_path = destination
            .not_locked_path()
            .join(Self::DEFAULT_BLOB_FILENAME);

        if blob_path.exists() {
            debug!("cache hit");
            return Ok(blob_path);
        }

        let blob = destination.open_exclusive(Self::DEFAULT_BLOB_FILENAME, self.ctx)?;

        let mut guard = guards::RemoveOnDrop::new(blob);

        self.ducknest_client
            .fetch_blob(package, guard.file())
            .await
            .with_context(|| {
                format!(
                    "while downloading a source of `{}` version {}",
                    package.name, package.version
                )
            })?;
        guard.disarm();
        Ok(blob_path)
    }

    #[tracing::instrument(skip(self))]
    fn acquire_package_lock(&self, pkg: types::Package) -> QuackResult<LockedFile> {
        self.locks_path
            .join(pkg.name)
            .join(pkg.version.to_string())
            .open_exclusive(Self::PER_PACKAGE_LOCK, self.ctx())
            .with_context(|| {
                format!(
                    "failed to acquire a lock for `{}` version {}",
                    pkg.name, pkg.version
                )
            })
    }

    /// Clone a git repository pointed by `source` to the `destination_directory`.
    #[tracing::instrument(skip(self))]
    pub fn clone_from_git_to_directory(
        &self,
        url: &Url,
        reference: GitReference,
        destination_directory: &std::path::Path,
    ) -> QuackResult<types::GitCloneResponse> {
        self.git_client
            .clone_blocking(url, reference, destination_directory)
            .with_context(|| format!("failed to clone the repository at `{url}`"))
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

    pub fn try_get_fastpath(
        &self,
        url: InternedUrl,
    ) -> Option<Box<dyn git::fast_path::GitFastPathExt + '_>> {
        if self.ctx().is_offline() {
            None
        } else {
            self.git_fastpath_client.try_get_client(url)
        }
    }

    /// Get the [`DuckContext`] used to construct this [`Fetcher`] instance.
    pub fn ctx(&self) -> &DuckContext {
        self.ctx
    }

    #[tracing::instrument(skip_all, fields(%retries, calls = retries + 1))]
    /// Fetch a package from ducknest with retries.
    pub async fn fetch_package_blob_with_retries(
        &self,
        pkg: PackageWithUrl,
        retries: u32,
    ) -> QuackResult<PathBuf> {
        debug!("fetching with retries");
        self.ctx.info(format!(
            "starting a download of `{}` version {} from `{}`",
            pkg.name, pkg.version, pkg.url
        ))?;
        let calls = retries + 1;
        for attempt in 1..=calls {
            match self.fetch_package_blob(pkg).await {
                Ok(path) => {
                    info!(%attempt, "fetched");
                    return Ok(path);
                }
                Err(e) => {
                    error!(error = %e, "failed to fetch");
                    let will_retry = attempt != calls;
                    if will_retry {
                        debug!(%attempt, "retrying fetch");
                    }
                    self.ctx.warning(format!(
                        "failed to download `{}` version {} from `{}`: {e}",
                        pkg.name, pkg.version, pkg.url
                    ))?;
                }
            }
        }

        let retries_string = if retries == 1 { "retry" } else { "retries" };

        qp_bail!(
            "failed to fetch a package {} {} after {retries} {retries_string}",
            pkg.name,
            pkg.version
        )
    }
}
