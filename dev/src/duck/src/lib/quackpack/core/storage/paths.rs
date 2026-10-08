// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! An abstraction over implementation details of storage.
//!
//! Layout:
//! <storage root>
//! ├── pkg/
//! │   ├── <package id>/
//! │   │   ├── <other files>
//! │   │   └── checksum.txt
//! │   └── ...
//! ├── venv/
//! │   ├── <venv name>/
//! │   │   ├── metadata
//! │   │   └── metadata.old
//! │   └── ...
//! └── locks/
//!     ├── clean.lock
//!     ├── venv_sync/
//!     │   ├── <package id>
//!     │   └── ...
//!     └── venv_data/
//!         ├── <package id>
//!         └── ...

use std::path::{Path, PathBuf};

use crate::quackpack::core::full_identity::FullKind;
use crate::quackpack::core::storage::DirContents;
use crate::quackpack::core::storage::venv_id::VenvId;
use crate::quackpack::core::{PackageId, Version, storage_name_for_git, storage_name_for_registry};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, qp_bail_internal};

const LOCKS_DIRECTORY_NAME: &str = "locks";
const VENV_SYNC_LOCK_FILENAME: &str = "venv_sync";
const VENV_DATA_LOCK_FILENAME: &str = "venv_data";
const CLEAN_LOCK_FILENAME: &str = "clean.lock";

const VENVS_DIR_NAME: &str = "venv";
const METADATA_FILENAME: &str = "metadata";
const BACKUP_METADATA_FILENAME: &str = "metadata.old";
const PKGS_DIR_NAME: &str = "pkg";

pub const OK_FILENAME: &str = ".ok";

#[derive(Debug, Clone)]
/// Provides paths of the storage components, hiding the implementation details of the directory layout.
pub struct Storage {
    root: PathBuf,
}

impl Storage {
    /// Create a new [`Storage`] rooted at `root`.
    pub fn new(root: impl Into<PathBuf>) -> Self {
        let root = root.into();
        Self { root }
    }

    /// Get the root of this storage.
    pub fn root(&self) -> &Path {
        &self.root
    }

    /// Get the root directory for storing packages.
    pub fn packages_root_dir(&self) -> PathBuf {
        self.root.join(PKGS_DIR_NAME)
    }

    /// Get the root directory for storing venvs.
    pub fn venvs_root_dir(&self) -> PathBuf {
        self.root.join(VENVS_DIR_NAME)
    }

    /// Get the root [`FileLockManager`] for storing locks.
    fn locks_base(&self) -> FileLockManager {
        FileLockManager::new(self.root.clone()).join(LOCKS_DIRECTORY_NAME)
    }

    #[track_caller]
    /// Get the root directory for storing a package.
    pub fn pkg_dir(&self, pkg_id: PackageId) -> PathBuf {
        match pkg_id.kind() {
            FullKind::Git { commit } => self.git_dir(pkg_id.url(), &commit),
            FullKind::Registry => self.registry_dir(&pkg_id.name(), pkg_id.version(), pkg_id.url()),
            FullKind::Local => {
                unreachable!("local packages should not be stored in storage `{self:?}`")
            }
        }
    }

    /// Get the root directory for storing a git package.
    pub fn git_dir(&self, url: InternedUrl, commit: &str) -> PathBuf {
        let pkg_dir = storage_name_for_git(url, commit);
        self.packages_root_dir().join(pkg_dir)
    }

    /// Get the root directory for storing a registry package.
    pub fn registry_dir(&self, name: &str, version: Version, url: InternedUrl) -> PathBuf {
        let pkg_dir = storage_name_for_registry(name, version, url);
        self.packages_root_dir().join(pkg_dir)
    }

    /// Try to acquire a shared clean lock.
    pub fn shared_clean_lock(&self) -> QuackResult<Option<LockedFile>> {
        self.locks_base().try_open_shared_rw(CLEAN_LOCK_FILENAME)
    }

    /// Acquire an exclusive clean lock.
    pub fn exclusive_clean_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.locks_base().open_exclusive(CLEAN_LOCK_FILENAME, ctx)
    }

    /// Get the [`FileLockManager`] for data storing locks.
    pub fn data_locks(&self) -> FileLockManager {
        self.locks_base().join(VENV_DATA_LOCK_FILENAME)
    }

    /// Get the root directory for data storing locks.
    pub fn data_locks_path(&self) -> PathBuf {
        self.data_locks().into_not_locked_path()
    }

    /// Get the [`FileLockManager`] for sync storing locks.
    pub fn sync_locks(&self) -> FileLockManager {
        self.locks_base().join(VENV_SYNC_LOCK_FILENAME)
    }

    /// Get the root directory for sync storing locks.
    pub fn sync_locks_path(&self) -> PathBuf {
        self.sync_locks().into_not_locked_path()
    }

    /// Get the root directory for storing venv `venv_id`.
    pub fn venv_dir(&self, venv_id: VenvId) -> PathBuf {
        self.venvs_root_dir().join(venv_id)
    }

    /// Get the path to the metadata of the venv `venv_id`.
    pub fn venv_metadata(&self, venv_id: VenvId) -> PathBuf {
        self.venv_dir(venv_id).join(METADATA_FILENAME)
    }

    /// Get the path to the metadata of the venv `venv_id`.
    pub fn venv_backup_metadata(&self, venv_id: VenvId) -> PathBuf {
        self.venv_dir(venv_id).join(BACKUP_METADATA_FILENAME)
    }

    /// Returns an iterator over all packages in the storage.
    /// The yielded packages need not be correct (may be missing checksum).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_pkgs(&self) -> QuackResult<DirContents> {
        create_dir_iterator(&self.packages_root_dir())
    }

    /// Returns an iterator over all venvs in the storage.
    /// The yielded venvs need not be correct (may have invalid data).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_venvs(&self) -> QuackResult<DirContents> {
        create_dir_iterator(&self.venvs_root_dir())
    }

    /// Returns an iterator over all sync locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_sync_locks(&self) -> QuackResult<DirContents> {
        create_dir_iterator(&self.sync_locks_path())
    }

    /// Returns an iterator over all data locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_data_locks(&self) -> QuackResult<DirContents> {
        create_dir_iterator(&self.data_locks_path())
    }

    /// Check if package is stored in storage.
    pub fn is_package_stored(&self, pkg_id: PackageId) -> bool {
        match pkg_id.kind() {
            FullKind::Git { commit } => self.is_stored_git(pkg_id.url(), &commit),
            FullKind::Registry => {
                self.is_stored_registry(&pkg_id.name(), pkg_id.version(), pkg_id.url())
            }
            FullKind::Local => false,
        }
    }

    /// Check if git package is stored in storage.
    pub fn is_stored_git(&self, url: InternedUrl, commit: &str) -> bool {
        let dir = self.git_dir(url, commit);
        dir.is_dir() && dir.join(OK_FILENAME).exists()
    }

    /// Check if repository package is stored in storage.
    pub fn is_stored_registry(&self, name: &str, version: Version, url: InternedUrl) -> bool {
        let dir = self.registry_dir(name, version, url);
        dir.is_dir() && dir.join(OK_FILENAME).exists()
    }

    /// Mark package as fully stored in storage.
    pub fn mark_as_stored(&self, pkg_id: PackageId) -> QuackResult<()> {
        match pkg_id.kind() {
            FullKind::Git { commit } => self.mark_git_stored(pkg_id.url(), &commit),
            FullKind::Registry => {
                self.mark_registry_stored(&pkg_id.name(), pkg_id.version(), pkg_id.url())
            }
            FullKind::Local => qp_bail_internal!("attempting to store a local package: {pkg_id:?}"),
        }
    }

    /// Mark git package as fully stored in storage.
    pub fn mark_git_stored(&self, url: InternedUrl, commit: &str) -> QuackResult<()> {
        let dir = self.git_dir(url, commit);
        dir.join(OK_FILENAME).touch()?;
        Ok(())
    }

    /// Mark repository package as fully stored in storage.
    pub fn mark_registry_stored(
        &self,
        name: &str,
        version: Version,
        url: InternedUrl,
    ) -> QuackResult<()> {
        let dir = self.registry_dir(name, version, url);
        dir.join(OK_FILENAME).touch()?;
        Ok(())
    }

    /// Attempts to remove a package from the storage,
    /// does not fail if such package is not stored.
    pub fn try_remove_pkg(&self, pkg_id: PackageId) -> QuackResult<()> {
        let pkg_dir = match pkg_id.kind() {
            FullKind::Git { commit } => self.git_dir(pkg_id.url(), &commit),
            FullKind::Registry => self.registry_dir(&pkg_id.name(), pkg_id.version(), pkg_id.url()),
            FullKind::Local => return Ok(()),
        };
        pkg_dir.rmtree()
    }
}

/// Returns iterator over files in a directory.
/// If the directory does not exist, returns an empty iterator.
fn create_dir_iterator(path: &Path) -> QuackResult<DirContents> {
    match path.read_dir() {
        Ok(read_dir) => Ok(DirContents::NonEmpty(read_dir)),
        Err(e) => match e.kind() {
            std::io::ErrorKind::NotFound => Ok(DirContents::Empty),
            _ => Err(e.into()),
        },
    }
}
