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
use std::{
    fs::ReadDir,
    path::{Path, PathBuf},
};

use crate::{QuackResult, StrId, duck::util::duck_home::DuckHome};

const LOCKS_DIRECTORY_NAME: &str = "locks";
const VENV_SYNC_LOCK_FILENAME: &str = "venv_sync";
const VENV_DATA_LOCK_FILENAME: &str = "venv_data";
const CLEAN_LOCK_FILENAME: &str = "clean.lock";

const VENVS_DIR_NAME: &str = "venv";
const METADATA_FILENAME: &str = "metadata";
const BACKUP_METADATA_FILENAME: &str = "metadata.old";
const PKG_DIR_NAME: &str = "pkg";

const CHECKSUM_FILENAME: &str = "checksum.txt";

#[derive(Debug)]
/// Provides paths of the storage components, hiding the implementation details of the directory layout.
pub(super) struct StoragePaths {
    root: PathBuf,
    package_dir: PathBuf,
    venv_dir: PathBuf,
    clean_lock: PathBuf,
    sync_lock_base: PathBuf,
    data_lock_base: PathBuf,
}

impl StoragePaths {
    pub(super) fn new(layout: &DuckHome) -> Self {
        let root = layout.storage_dir().to_path_buf();

        let package_dir = root.join(PKG_DIR_NAME);
        let venv_dir = root.join(VENVS_DIR_NAME);

        let lock_base = root.join(LOCKS_DIRECTORY_NAME);
        let clean_lock = lock_base.join(CLEAN_LOCK_FILENAME);
        let sync_lock_base = lock_base.join(VENV_SYNC_LOCK_FILENAME);
        let data_lock_base = lock_base.join(VENV_DATA_LOCK_FILENAME);
        Self {
            root,
            package_dir,
            venv_dir,
            clean_lock,
            sync_lock_base,
            data_lock_base,
        }
    }

    pub(super) fn clean_lock(&self) -> &Path {
        &self.clean_lock
    }

    pub(super) fn venv_dir(&self, venv_id: StrId) -> PathBuf {
        self.venv_dir.join(venv_id)
    }

    pub(super) fn vevn_metadata(&self, venv_id: StrId) -> PathBuf {
        self.venv_dir(venv_id).join(METADATA_FILENAME)
    }

    pub(super) fn vevn_backup_metadata(&self, venv_id: StrId) -> PathBuf {
        self.venv_dir(venv_id).join(BACKUP_METADATA_FILENAME)
    }

    /// Returns an iterator over all packages in the storage.
    /// The yielded packages need not be correct (may be missing checksum).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub(super) fn iter_pkgs(&self) -> Option<QuackResult<ReadDir>> {
        let dir = self.package_dir.as_path();
        if !dir.is_dir() {
            None
        } else {
            Some(dir.read_dir().map_err(Into::into))
        }
    }

    /// Returns an iterator over all venvs in the storage.
    /// The yielded venvs need not be correct (may have invalid data).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub(super) fn iter_vens(&self) -> Option<QuackResult<ReadDir>> {
        let dir = self.venv_dir.as_path();
        if !dir.is_dir() {
            None
        } else {
            Some(dir.read_dir().map_err(Into::into))
        }
    }

    /// Returns an iterator over all sync locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub(super) fn iter_sync_locks(&self) -> Option<QuackResult<ReadDir>> {
        let dir = self.sync_lock_base.as_path();
        if !dir.is_dir() {
            None
        } else {
            Some(dir.read_dir().map_err(Into::into))
        }
    }

    /// Returns an iterator over all data locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub(super) fn iter_data_locks(&self) -> Option<QuackResult<ReadDir>> {
        let dir = self.data_lock_base.as_path();
        if !dir.is_dir() {
            None
        } else {
            Some(dir.read_dir().map_err(Into::into))
        }
    }
}
