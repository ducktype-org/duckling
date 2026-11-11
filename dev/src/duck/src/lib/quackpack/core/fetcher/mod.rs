// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! [`Fetcher`] manages all network-related clients.
//!
//! It incorporates [`DucknestClient`](ducknest::DucknestClient) with
//! [`ManifestCache`](cache::ManifestCache), and provides another layer of abstraction over the
//! [`GitClient`](git::GitClient).

use std::cell::{OnceCell, RefCell};
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::LazyLock;

use tempfile::TempDir;
use tracing::debug;
use url::Url;

use crate::quackpack::core::fetcher::http_async::AsyncHttpClient;
use crate::quackpack::core::fetcher::types::{FetcherResponse, PackageWithUrl};
use crate::quackpack::core::{GitReference, PackageLoader};
use crate::quackpack::schemas::registry;
use crate::quackpack::util::guards;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::util::file_locks::LockedFile;
use crate::util::once_ext::OnceExt;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId, qp_bail};

pub mod cache;
pub mod downloads;
pub mod ducknest;
pub mod git;
pub mod http;
pub mod http_async;
pub mod locks;
pub mod types;
pub mod util;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// A class for managing HTTP and Git clients, and caching metadata.
pub struct Fetcher<'duck> {
    ctx: &'duck DuckContext,
    ducknest_client: OnceCell<ducknest::DucknestClient<'duck>>,
    http_client: OnceCell<Rc<AsyncHttpClient<'duck>>>,
    git_client: OnceCell<Box<dyn git::GitClient + 'duck>>,
    git_fastpath_client: OnceCell<git::fast_path::GitFastPathClient<'duck>>,
    /// SQL cache is not lazily initialized, as failures leave `OnceCell` initialized, which makes
    /// every call to [`get_package_metadata`]/[`get_package_all_metadata`]:
    /// * try to initialize SQL cache (again),
    /// * fail.
    ///
    /// [`get_package_metadata`]: Self::get_package_metadata
    /// [`get_package_all_metadata`]: Self::get_package_all_metadata
    cache: RefCell<cache::ManifestCache>,
    downloads: OnceCell<downloads::Downloads>,
    locks: OnceCell<locks::Locks>,
}

/// URL of the default Ducknest instance.
/// It should be used only by [`DuckCfg::registry_url`] or tests (as it's the case during writing
/// this comment; we don't (yet) have any fast paths for the _real_ default registry).
///
/// [`DuckCfg::registry_url`]: crate::duck::util::duck_cfg::DuckCfg::registry_url
pub static DEFAULT_REGISTRY_URL: LazyLock<InternedUrl> = LazyLock::new(|| {
    InternedUrl::new(
        "http://localhost:9001"
            .parse()
            .expect("compile time known value"),
    )
});

impl<'duck> Fetcher<'duck> {
    /// Create a new [`Fetcher`].
    pub fn new(ctx: &'duck DuckContext) -> QuackResult<Self> {
        let metadata_path = ctx.duck_home().get_metadata_db_path();
        let cache_location = if cfg!(test) {
            // For tests, we want to use in-memory database.
            cache::CacheLocation::Memory
        } else {
            cache::CacheLocation::Path(metadata_path.as_path())
        };
        let cache = cache::ManifestCache::new(cache_location)
            .context("failed to create an SQLite3 cache client")?;

        Ok(Self {
            ctx,
            http_client: OnceCell::new(),
            ducknest_client: OnceCell::new(),
            git_client: OnceCell::new(),
            git_fastpath_client: OnceCell::new(),
            cache: RefCell::new(cache),
            downloads: OnceCell::new(),
            locks: OnceCell::new(),
        })
    }

    /// Retrieve metadata for `package` from a given Ducknest instance.
    ///
    /// Exact cache hit takes precedence over HTTP requests.
    #[tracing::instrument(target = "duck::ducknest", level = "debug", skip(self))]
    pub async fn get_package_metadata(
        &self,
        package: types::PackageWithUrl,
    ) -> QuackResult<FetcherResponse<registry::Manifest>> {
        if let Some(cached) = self.cache.borrow().get_manifest(package)? {
            debug!(target: "duck::ducknest", "metadata cache hit");
            return Ok(FetcherResponse::Some(cached));
        }
        debug!(target: "duck::ducknest", "metadata cache miss");
        if self.ctx.is_offline() {
            return Ok(FetcherResponse::Offline);
        }
        let result = self
            .ducknest_client()
            .get_exact_metadata(package)
            .await
            .with_context(|| {
                format!(
                    "failed to get the metadata of `{}` version {}",
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
    #[tracing::instrument(target = "duck::ducknest", level = "debug", skip(self))]
    pub async fn get_package_all_metadata(
        &self,
        url: InternedUrl,
        package_name: StrId,
    ) -> QuackResult<FetcherResponse<types::MultiMetadata>> {
        if self.ctx.is_offline() {
            debug!(target: "duck::ducknest", "offline, using only the cached metadata");
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
            .ducknest_client()
            .get_multi_metadata(&url, package_name)
            .await
            .with_context(|| format!("failed to get the multimetadata of `{}`", package_name))?;
        self.cache
            .borrow_mut()
            .add_or_replace_multiple_manifests(url, result.packages_metadata.clone())?;
        Ok(FetcherResponse::Some(result))
    }

    /// Fetch a source of a `package`. Returns a path to the file where the blob has been saved.
    async fn fetch_package_blob(
        &self,
        package: types::PackageWithUrl,
        file: &LockedFile,
    ) -> QuackResult<()> {
        self.ducknest_client()
            .fetch_blob(package, file)
            .await
            .with_context(|| {
                format!(
                    "failed to download the source of `{}` version {}",
                    package.name, package.version
                )
            })
    }

    /// Clone a git repository pointed by `url` to the `destination_directory`.
    #[tracing::instrument(target = "duck::git", level = "debug", skip(self, url), fields(%url))]
    #[track_caller] // Because of `.unwrap_package()`.
    pub fn clone_from_git_to_directory(
        &self,
        url: &Url,
        reference: GitReference,
        destination_directory: &Path,
    ) -> QuackResult<types::ClonedPackage> {
        let client = self.git_client()?;
        self.ctx.info(format!("cloning a repository at `{url}`"))?;

        let commit = self
            .clone_from_git_to_directory_inner(client, url, reference, destination_directory)
            .map_err(|err| add_hint_for_using_git_cli(client, err))
            .with_context(|| format!("failed to clone the package from `{url}`"))?;
        let package = PackageLoader::find_at_exact_directory(destination_directory, self.ctx)
            .with_context(|| {
                format!("failed to parse a package from repository cloned from `{url}`")
            })?
            .into_package()
            .unwrap_package();
        Ok(types::ClonedPackage { commit, package })
    }

    /// Helper for [`clone_from_git_to_directory`], to make attaching contexts easier.
    /// Returns commit of the cloned repo.
    ///
    /// [`clone_from_git_to_directory`]: Self::clone_from_git_to_directory
    fn clone_from_git_to_directory_inner(
        &self,
        client: &dyn git::GitClient,
        url: &Url,
        reference: GitReference,
        destination_directory: &Path,
    ) -> QuackResult<StrId> {
        let repository = client
            .clone(url, reference, destination_directory)
            .with_context(|| format!("failed to clone the repository from `{url}`"))?;
        repository.checkout(reference).with_context(|| {
            format!("failed to checkout the repository cloned from `{url}` to the {reference}")
        })?;
        repository
            .init_submodules(git::GitSubmoduleOptions::Update {
                init: true,
                recursive: true,
            })
            .with_context(|| {
                format!("failed to update submodules of the repository cloned from `{url}`")
            })?;
        let commit = repository
            .head_commit()
            .context("failed to get HEAD commit of a repository")?;
        Ok(commit)
    }

    /// Same as [`clone_from_git_to_directory`](Self::clone_from_git_to_directory), but a target directory is a temporary
    /// directory.
    ///
    /// This is needed because storage paths depend on a commit, which we can only get after
    /// cloning a repository.
    pub fn clone_from_git(
        &self,
        url: &Url,
        reference: GitReference,
    ) -> QuackResult<(types::ClonedPackage, TempDir)> {
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
            self.git_fastpath_client().try_get_client(url)
        }
    }

    /// Get the [`DuckContext`] used to construct this [`Fetcher`] instance.
    pub fn ctx(&self) -> &DuckContext {
        self.ctx
    }

    #[tracing::instrument(target = "duck::ducknest", level = "debug", skip(self))]
    /// Fetch a package from ducknest with retries.
    pub async fn fetch_package_blob_with_retries(
        &self,
        package: PackageWithUrl,
        retries: u32,
    ) -> QuackResult<PathBuf> {
        self.ctx.info(format!(
            "starting a download of `{}` version {} from `{}`",
            package.name, package.version, package.url
        ))?;

        // Firstly acquire a "global" per package lock.
        // Otherwise, under the same global fetcher lock, we can start two downloads of the same package,
        // and one can see existing but empty path.
        let _lock = self.locks()?.lock(package, self.ctx)?;
        let entry = self.downloads()?.get(package, self.ctx)?;

        let blob = match entry {
            downloads::DownloadedEntry::Present(path) => return Ok(path),
            downloads::DownloadedEntry::Vacant(blob) => blob,
        };

        let mut guard = guards::RemoveOnDrop::new(blob);

        let calls = retries + 1;
        for attempt in 1..=calls {
            match self.fetch_package_blob(package, guard.file()).await {
                Ok(()) => {
                    debug!(target: "duck::ducknest", "fetched on attempt {attempt}/{calls}");
                    guard.disarm();
                    return Ok(guard.file().path().to_path_buf());
                }
                Err(e) => {
                    debug!(target: "duck::ducknest", error = %e, "attempt {attempt}/{calls} failed");
                    self.ctx.warning(format!(
                        "failed to download `{}` version {} from `{}`: {e}",
                        package.name, package.version, package.url
                    ))?;
                }
            }
        }

        let retries_string = if retries == 1 { "retry" } else { "retries" };

        qp_bail!(
            "failed to fetch a package {} {} after {retries} {retries_string}",
            package.name,
            package.version
        )
    }

    /////////////////////////////////////////////////////////////////////////////////////////////
    ////                      `OnceCell` helpers and getters                                 ////
    /////////////////////////////////////////////////////////////////////////////////////////////

    /// Get or initialize a new [`AsyncHttpClient`].
    fn http_client(&self) -> &Rc<AsyncHttpClient<'duck>> {
        self.http_client
            .get_or_init(|| Rc::new(AsyncHttpClient::new(self.ctx)))
    }

    /// Get or initialize a new [`downloads::Downloads`].
    fn downloads(&self) -> QuackResult<&downloads::Downloads> {
        self.downloads.try_init_with(|| {
            let download_cache_path = self.ctx.duck_home().downloads();
            downloads::Downloads::new(download_cache_path)
                .context("failed to create a downloads manager")
        })
    }

    /// Get or initialize a new [`locks::Locks`].
    fn locks(&self) -> QuackResult<&locks::Locks> {
        self.locks.try_init_with(|| {
            let locks = self.ctx.duck_home().cache().join("locks");
            locks::Locks::new(locks).context("failed to create a fetcher locks manager")
        })
    }

    /// Get or initialize a new [`ducknest::DucknestClient`].
    fn ducknest_client(&self) -> &ducknest::DucknestClient<'duck> {
        self.ducknest_client
            .get_or_init(|| ducknest::DucknestClient::new(self.http_client().clone()))
    }

    /// Get or initialize a new [`git::GitClient`].
    /// It chooses the right client based on the user configuration.
    fn git_client(&self) -> QuackResult<&(dyn git::GitClient + 'duck)> {
        self.git_client
            .try_init_with(|| {
                let network_config = self.ctx.duck_cfg().network()?;
                let client: Box<dyn git::GitClient + 'duck> =
                    if network_config.use_git_cli == Some(true) {
                        debug!(target: "duck::git", "using git cli backend");
                        Box::new(git::cli::GitCliClient::new()?)
                    } else {
                        debug!(target: "duck::git", "using git2 backend");
                        Box::new(git::git2::Git2Client::new(self.ctx))
                    };
                Ok(client)
            })
            .map(|b| &**b)
    }

    /// Get or initialize a new [`git::fast_path::GitFastPathClient`].
    fn git_fastpath_client(&self) -> &git::fast_path::GitFastPathClient<'duck> {
        self.git_fastpath_client
            .get_or_init(|| git::fast_path::GitFastPathClient::new(self.http_client().clone()))
    }
}

fn add_hint_for_using_git_cli(client: &dyn git::GitClient, mut error: QuackError) -> QuackError {
    if client.uses_cli() {
        return error;
    }
    error.add_hint("set `network.use-git-cli: true` in `config.yaml` to use the `git` binary");
    error
}
