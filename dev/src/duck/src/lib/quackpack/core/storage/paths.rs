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
//!     ├── compile/
//!     │   ├── <package id>
//!     │   └── ...
//!     ├── venv_sync/
//!     │   ├── <package id>
//!     │   └── ...
//!     └── venv_data/
//!         ├── <package id>
//!         └── ...

use std::fs::ReadDir;
use std::path::{Path, PathBuf};

use super::package_id::PackageId;
use crate::quackpack::core::storage::venv_id::VenvId;
use crate::util::filesystem::{Filesystem, LockedFile};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, qp_bail_internal};

const LOCKS_DIRECTORY_NAME: &str = "locks";
const VENV_SYNC_LOCK_FILENAME: &str = "venv_sync";
const VENV_DATA_LOCK_FILENAME: &str = "venv_data";
const COMPILE_LOCK_FILENAME: &str = "compile";
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
    pub fn packages_dir(&self) -> PathBuf {
        self.root.join(PKGS_DIR_NAME)
    }

    /// Get the root directory for storing venvs.
    pub fn venvs_dir(&self) -> PathBuf {
        self.root.join(VENVS_DIR_NAME)
    }

    /// Get the root directory for storing locks.
    fn locks_base(&self) -> Filesystem {
        Filesystem::new(self.root.clone()).join(LOCKS_DIRECTORY_NAME)
    }

    /// Get the root directory for storing package `package`.
    pub fn pkg_dir(&self, package: &PackageId) -> PathBuf {
        self.packages_dir().join(package.storage_name())
    }

    /// Acquire a shared clean lock.
    pub fn shared_clean_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.locks_base()
            .open_shared_rw_create(CLEAN_LOCK_FILENAME, ctx)
    }

    /// Acquire an exclusive clean lock.
    pub fn exclusive_clean_lock(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.locks_base().open_exclusive(CLEAN_LOCK_FILENAME, ctx)
    }

    /// Get the root directory for data storing locks.
    pub fn data_locks(&self) -> Filesystem {
        self.locks_base().join(VENV_DATA_LOCK_FILENAME)
    }

    /// Get the root directory for sync storing locks.
    pub fn sync_locks(&self) -> Filesystem {
        self.locks_base().join(VENV_SYNC_LOCK_FILENAME)
    }

    /// Get the root directory for compile storing locks.
    pub fn compile_locks(&self) -> Filesystem {
        self.locks_base().join(COMPILE_LOCK_FILENAME)
    }

    /// Get the root directory for storing venv `venv_id`.
    pub fn venv_dir(&self, venv_id: VenvId) -> PathBuf {
        self.venvs_dir().join(venv_id)
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
    pub fn iter_pkgs(&self) -> QuackResult<ReadDir> {
        let dir = self.packages_dir();
        create_dir_iterator(&dir)
    }

    /// Returns an iterator over all venvs in the storage.
    /// The yielded venvs need not be correct (may have invalid data).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_venvs(&self) -> QuackResult<ReadDir> {
        let dir = self.venvs_dir();
        create_dir_iterator(&dir)
    }

    /// Returns an iterator over all sync locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_sync_locks(&self) -> QuackResult<ReadDir> {
        let dir = self.sync_locks().into_not_locked_path();
        create_dir_iterator(&dir)
    }

    /// Returns an iterator over all data locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_data_locks(&self) -> QuackResult<ReadDir> {
        let dir = self.data_locks().into_not_locked_path();
        create_dir_iterator(&dir)
    }

    /// Returns an iterator over all compile locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_compile_locks(&self) -> QuackResult<ReadDir> {
        let dir = self.compile_locks().into_not_locked_path();
        create_dir_iterator(&dir)
    }

    /// Check if package `id` is stored in storage.
    pub fn is_package_stored(&self, id: &PackageId) -> bool {
        if id.is_local() {
            return false;
        }
        let dir = self.pkg_dir(id);
        dir.is_dir() && dir.join(OK_FILENAME).exists()
    }

    /// Mark package `id` as fully stored in storage.
    pub fn mark_as_stored(&self, id: &PackageId) -> QuackResult<()> {
        if id.is_local() {
            qp_bail_internal!("attempting to store a local package")
        }
        let dir = self.pkg_dir(id);
        dir.join(OK_FILENAME).touch()?;
        Ok(())
    }
}

fn create_dir_iterator(path: &Path) -> QuackResult<ReadDir> {
    path.read_dir().map_err(Into::into)
}
