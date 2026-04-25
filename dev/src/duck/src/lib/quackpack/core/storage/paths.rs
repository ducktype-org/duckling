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
use crate::util::path_ops_ext::PathOpsExt;
use crate::{QuackResult, qp_bail_internal};

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
    packages_dir: PathBuf,
    venvs_dir: PathBuf,
    clean_lock: PathBuf,
    sync_lock_base: PathBuf,
    data_lock_base: PathBuf,
    compile_locks_base: PathBuf,
}

impl Storage {
    pub fn new(root: impl Into<PathBuf>) -> Self {
        let root = root.into();
        let packages_dir = root.join(PKGS_DIR_NAME);
        let venvs_dir = root.join(VENVS_DIR_NAME);

        let lock_base = root.join(LOCKS_DIRECTORY_NAME);
        let clean_lock = lock_base.join(CLEAN_LOCK_FILENAME);
        let sync_lock_base = lock_base.join(VENV_SYNC_LOCK_FILENAME);
        let data_lock_base = lock_base.join(VENV_DATA_LOCK_FILENAME);
        let compile_locks_base = lock_base.join(COMPILE_LOCK_FILENAME);
        Self {
            root,
            packages_dir,
            venvs_dir,
            clean_lock,
            sync_lock_base,
            data_lock_base,
            compile_locks_base,
        }
    }

    pub fn root(&self) -> &Path {
        &self.root
    }

    pub fn pkg_dir(&self, package: &PackageId) -> PathBuf {
        self.packages_dir.join(package.storage_name())
    }

    pub fn clean_lock(&self) -> &Path {
        &self.clean_lock
    }

    pub fn sync_lock(&self, venv_id: VenvId) -> PathBuf {
        self.sync_lock_base.join(venv_id)
    }

    pub fn data_lock(&self, venv_id: VenvId) -> PathBuf {
        self.data_lock_base.join(venv_id)
    }

    pub fn compile_lock(&self, venv_id: VenvId) -> PathBuf {
        self.compile_locks_base.join(venv_id)
    }

    pub fn venv_dir(&self, venv_id: VenvId) -> PathBuf {
        self.venvs_dir.join(venv_id)
    }

    pub fn venv_metadata(&self, venv_id: VenvId) -> PathBuf {
        self.venv_dir(venv_id).join(METADATA_FILENAME)
    }

    pub fn venv_backup_metadata(&self, venv_id: VenvId) -> PathBuf {
        self.venv_dir(venv_id).join(BACKUP_METADATA_FILENAME)
    }

    /// Returns an iterator over all packages in the storage.
    /// The yielded packages need not be correct (may be missing checksum).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_pkgs(&self) -> QuackResult<ReadDir> {
        let dir = self.packages_dir.as_path();
        create_dir_iterator(dir)
    }

    /// Returns an iterator over all venvs in the storage.
    /// The yielded venvs need not be correct (may have invalid data).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_venvs(&self) -> QuackResult<ReadDir> {
        let dir = self.venvs_dir.as_path();
        create_dir_iterator(dir)
    }

    /// Returns an iterator over all sync locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_sync_locks(&self) -> QuackResult<ReadDir> {
        let dir = self.sync_lock_base.as_path();
        create_dir_iterator(dir)
    }

    /// Returns an iterator over all data locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_data_locks(&self) -> QuackResult<ReadDir> {
        let dir = self.data_lock_base.as_path();
        create_dir_iterator(dir)
    }

    /// Returns an iterator over all compile locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_compile_locks(&self) -> QuackResult<ReadDir> {
        let dir = self.compile_locks_base.as_path();
        create_dir_iterator(dir)
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

    pub fn packages_base_dir(&self) -> &Path {
        &self.packages_dir
    }

    pub fn venvs_base_dir(&self) -> &Path {
        &self.venvs_dir
    }
}

fn create_dir_iterator(path: &Path) -> QuackResult<ReadDir> {
    path.read_dir().map_err(Into::into)
}
