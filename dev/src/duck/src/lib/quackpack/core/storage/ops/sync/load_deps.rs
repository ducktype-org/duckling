//! Module for loading (and firstly downloading if absent) packages of the freeze generated during `sync`.
use std::cell::RefCell;
use std::fs::File;
use std::path::{Path, PathBuf};

use flate2::read::GzDecoder;
use futures::executor::block_on;
use futures::{StreamExt, stream};
use tar::Archive;
use tracing::{debug, warn};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::fetcher::types::PackageWithUrl;
use crate::quackpack::core::full_identity::FullKind;
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::{AnyPackage, GitReference, PackageId, PackageLoader};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_path_buf::ToPathBuf;
use crate::util::Pluralize;
use crate::util::error::ErrorsLogger;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal};

// We want at most 3 calls, therefore we retry 2 times.
/// The maximal number of retries when trying to download a package blob from registry.
const MAX_BLOB_RETRY_COUNT: u32 = 2;

#[derive(Debug, Clone)]
/// A struct indicating a successful load of a package.
pub struct SuccessfullyLoadedPackage {
    /// Id of the package for which source code was fetched.
    pub id: PackageId,
    /// The fetched package.
    pub pkg: AnyPackage,
    /// Was the source code already present or did we have to download it.
    pub was_present: bool,
}

#[derive(Debug, Clone, Default)]
/// A result of the procedure of loading dependencies.
pub struct LoadedFreezePackages {
    /// Pairing between dependencies and fetched contents viewed as packages.
    pub _pkgs: Vec<(PackageId, AnyPackage)>,
    /// Number of packages which source codes had to be downloaded, used for user messages.
    pub freshly_downloaded_num: usize,
    /// Number of packages which were already downloaded.
    pub already_present_num: usize,
}

/// Load packages from the freeze, firstly fetching them is they are not present on the machine.
/// This means downloading them if they have not yet been downloaded,
/// and loading them from storage as packages.
///
/// Note
/// ----
/// We use [`ErrorsLogger`] to delay bailing, downloading as many dependencies as possible.
#[tracing::instrument(skip_all)]
pub fn load_packages_in_freeze(
    storage: &Storage,
    fetcher: &Fetcher<'_>,
    pkgs: Vec<PackageId>,
) -> QuackResult<LoadedFreezePackages> {
    if pkgs.is_empty() {
        warn!("requested to load 0 packages");
        return Ok(LoadedFreezePackages::default());
    }
    let count = pkgs.len();
    fetcher.ctx().console().info(format!(
        "starting loading {count} package{}",
        count.s_if_plural()
    ))?;
    let fetcher_lock = fetcher
        .ctx()
        .duck_home()
        .open_fetcher_lockfile(fetcher.ctx())?;
    let max_connections = fetcher.ctx().max_open_connections();
    let logger = RefCell::new(ErrorsLogger::default());
    let fetches = stream::iter(pkgs)
        .map(|pkg| load_package(storage, fetcher, pkg, &logger))
        .buffer_unordered(max_connections)
        .collect::<Vec<_>>();
    let fetches = block_on(fetches);
    drop(fetcher_lock);
    bail_if_failed_to_fetch(fetcher.ctx(), logger.take())?;
    let mut pkgs = Vec::new();
    let mut freshly_downloaded_num = 0;
    let mut already_present_num = 0;
    for fetch in fetches.into_iter().flatten() {
        if fetch.was_present {
            already_present_num += 1;
        } else {
            freshly_downloaded_num += 1;
        }
        pkgs.push((fetch.id, fetch.pkg));
    }
    Ok(LoadedFreezePackages {
        _pkgs: pkgs,
        freshly_downloaded_num,
        already_present_num,
    })
}

/// Bail if we failed to download some package.
fn bail_if_failed_to_fetch(ctx: &DuckContext, logger: ErrorsLogger) -> QuackResult<()> {
    if logger.is_empty() {
        return Ok(());
    }
    let failed_count = logger.logged_errors();
    for fail in logger {
        ctx.error_console().error(fail)?;
    }
    qp_bail!(
        "failed to fetch {failed_count} package{}",
        failed_count.s_if_plural()
    )
}

/// Helper for [`load_packages_in_freeze`].
/// Loads a package, downloading it if not present and loading it from storage (except for local packages).
#[tracing::instrument(skip_all, fields(?pkg_id))]
async fn load_package(
    storage: &Storage,
    fetcher: &Fetcher<'_>,
    pkg_id: PackageId,
    logger: &RefCell<ErrorsLogger>,
) -> Option<SuccessfullyLoadedPackage> {
    debug!("fetching package");
    // Logs a result into the `logger`.
    macro_rules! log {
        ($e:expr) => {
            logger.borrow_mut().log_result($e)?
        };
    }
    // Logs a result into the `logger`.
    // If the result is an error, tries to remove the malformed package from storage.
    macro_rules! rm_and_log {
        ($e:expr) => {{
            let result = $e;
            let mut logger = logger.borrow_mut();
            if result.is_err() {
                logger.log_result(storage.try_remove_pkg(pkg_id).with_context(|| {
                    format!(
                        "when trying to remove a malformed dependency {} {} from its storage",
                        pkg_id.name(),
                        pkg_id.version()
                    )
                }));
            }
            logger.log_result(result)?
        }};
    }
    let url = pkg_id.url();
    let (pkg_dir, was_present) = match pkg_id.kind() {
        FullKind::Local => (
            log!(
                url.to_path_buf()
                    .with_context(|| format!("when loading a local dependency {}", pkg_id.name()))
            ),
            true,
        ),
        FullKind::Git { commit } => {
            rm_and_log!(fetch_git(pkg_id, url, commit, storage, fetcher))
        }
        FullKind::Registry => {
            let maybe_fetched = fetch_registry(pkg_id, url, storage, fetcher).await;
            rm_and_log!(maybe_fetched)
        }
    };
    let pkg = rm_and_log!(
        PackageLoader::find_at_exact_directory(&pkg_dir, fetcher.ctx())
            .with_context(|| malformed_dependency_msg(pkg_id, &pkg_dir, url))
    )
    .into_package();
    rm_and_log!(check_metadata(pkg_id, &pkg));
    Some(SuccessfullyLoadedPackage {
        id: pkg_id,
        pkg,
        was_present,
    })
}

/// Helper for [`load_package`].
/// Checks if the git package is stored in storage, if not clones it.
/// Returns path to the package in storage and whether it had to be cloned.
#[tracing::instrument(skip_all)]
fn fetch_git(
    pkg_id: PackageId,
    url: InternedUrl,
    commit: StrId,
    storage: &Storage,
    fetcher: &Fetcher<'_>,
) -> QuackResult<(PathBuf, bool)> {
    let was_present = storage.is_stored_git(url, &commit);
    if !was_present {
        fetcher.clone_from_git_to_directory(
            &url,
            GitReference::Rev(commit),
            &storage.git_dir(url, &commit),
        )?;
        mark_cloned_git_as_stored(pkg_id, storage).with_context(|| {
            format!(
                "failed to store a cloned repository of a package `{}`",
                pkg_id.value().as_identity()
            )
        })?;
    }
    Ok((storage.pkg_dir(pkg_id), was_present))
}

/// Mark a cloned git as stored.
fn mark_cloned_git_as_stored(pkg: PackageId, storage: &Storage) -> QuackResult<()> {
    let FullKind::Git { commit } = pkg.kind() else {
        qp_bail_internal!("attempted to store not a git package: {pkg:?}")
    };

    let url = pkg.url();
    storage.mark_as_stored(pkg)?;
    storage.git_dir(url, &commit).try_fsync_dir()?;
    Ok(())
}

/// Helper for [`load_package`].
/// Checks if the registry package is stored in storage, if not downloads it.
/// Returns path to the package in storage and whether it had to be downloaded.
async fn fetch_registry(
    pkg_id: PackageId,
    url: InternedUrl,
    storage: &Storage,
    fetcher: &Fetcher<'_>,
) -> QuackResult<(PathBuf, bool)> {
    let was_present = storage.is_package_stored(pkg_id);
    if !was_present {
        let fetcher_package = PackageWithUrl {
            name: pkg_id.name(),
            version: pkg_id.version(),
            url,
        };
        let blob_path = fetcher
            .fetch_package_blob_with_retries(fetcher_package, MAX_BLOB_RETRY_COUNT)
            .await?;
        unpack_package_blob(pkg_id, storage, &blob_path).with_context(|| {
            format!(
                "failed to unpack a compressed source code `{}` of a package `{}`",
                blob_path.display(),
                pkg_id.value().as_identity()
            )
        })?;
    }
    Ok((storage.pkg_dir(pkg_id), was_present))
}

/// Unpack a fetched package blob.
fn unpack_package_blob(pkg: PackageId, storage: &Storage, blob_path: &Path) -> QuackResult<()> {
    let pkg_dir = storage.pkg_dir(pkg);
    storage.try_remove_pkg(pkg)?;
    let file = File::open(blob_path)
        .with_context(|| format!("failed to open `{}`", blob_path.display()))?;
    let decompressed = GzDecoder::new(file);
    let mut archive = Archive::new(decompressed);
    archive.unpack(&pkg_dir).with_context(|| {
        format!(
            "failed to unpack `{}` to `{}`",
            blob_path.display(),
            pkg_dir.display()
        )
    })?;
    storage.mark_as_stored(pkg)?;
    pkg_dir.try_fsync_dir()?;
    Ok(())
}

/// Generate error message that the downloaded dependency is malformed (fails to load).
fn malformed_dependency_msg(pkg_id: PackageId, pkg_dir: &Path, url: InternedUrl) -> String {
    match pkg_id.kind() {
        FullKind::Registry => format!(
            "downloaded malformed dependency `{}` from `{}`",
            Into::<Identity>::into(*pkg_id.value()),
            url
        ),
        FullKind::Git { commit: _ } => format!(
            "cloned malformed dependency `{}` from `{}`",
            Into::<Identity>::into(*pkg_id.value()),
            url
        ),
        FullKind::Local => format!(
            "malformed local dependency `{}` at `{}`",
            Into::<Identity>::into(*pkg_id.value()),
            pkg_dir.display(),
        ),
    }
}

/// Check that the downloaded or loaded package has the desired name and version.
fn check_metadata(id: PackageId, package: &AnyPackage) -> QuackResult<()> {
    if package.name() == *id.name() && package.version() == id.version() {
        return Ok(());
    }
    let expected_package_str = format_args!("{} {}", id.name(), id.version());
    let found_package_str = format_args!("{} {}", package.name(), package.version());
    match id.kind() {
        FullKind::Registry => qp_bail!(
            "downloaded malformed dependency from `{}`: got name `{}`, expected `{}`",
            id.url(),
            expected_package_str,
            found_package_str
        ),
        FullKind::Git { commit: _ } => qp_bail!(
            "cloned malformed dependency from `{}`: got name `{}`, expected `{}`",
            id.url(),
            expected_package_str,
            found_package_str
        ),
        FullKind::Local => qp_bail!(
            "malformed local dependency at `{}`: got name `{}`, expected `{}`",
            id.url().to_path_buf()?.display(),
            expected_package_str,
            found_package_str
        ),
    }
}
