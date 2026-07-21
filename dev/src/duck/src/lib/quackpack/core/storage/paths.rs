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

use super::package_id::PackageId;
use crate::quackpack::core::storage::DirContents;
use crate::quackpack::core::storage::venv_id::VenvId;
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

const OK_FILENAME: &str = ".ok";

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

    /// Get the root directory for storing package `package`.
    pub fn pkg_dir(&self, package: PackageId) -> PathBuf {
        self.packages_root_dir().join(package.storage_name())
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

    /// Check if package `id` is stored in storage.
    pub fn is_package_stored(&self, id: PackageId) -> bool {
        if id.is_local() {
            return false;
        }
        let dir = self.pkg_dir(id);
        dir.is_dir() && dir.join(OK_FILENAME).exists()
    }

    /// Mark package `id` as fully stored in storage.
    pub fn mark_as_stored(&self, id: PackageId) -> QuackResult<()> {
        if id.is_local() {
            qp_bail_internal!("attempting to store a local package")
        }
        let dir = self.pkg_dir(id);
        dir.join(OK_FILENAME).touch()?;
        Ok(())
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
