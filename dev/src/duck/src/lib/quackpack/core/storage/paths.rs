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
    fs::DirEntry,
    io,
    path::{Path, PathBuf},
};

use rustvil::fs::PathExt;

use crate::{QuackResult, StrId, duck::util::duck_home::DuckHome, qp_bail_internal};

use super::package_id::PackageId;

const LOCKS_DIRECTORY_NAME: &str = "locks";
const VENV_SYNC_LOCK_FILENAME: &str = "venv_sync";
const VENV_DATA_LOCK_FILENAME: &str = "venv_data";
const CLEAN_LOCK_FILENAME: &str = "clean.lock";

const VENVS_DIR_NAME: &str = "venv";
const METADATA_FILENAME: &str = "metadata";
const BACKUP_METADATA_FILENAME: &str = "metadata.old";
const PKG_DIR_NAME: &str = "pkg";

const OK_FILENAME: &str = ".ok";

#[derive(Debug)]
/// Provides paths of the storage components, hiding the implementation details of the directory layout.
pub struct StoragePaths {
    package_dir: PathBuf,
    venv_dir: PathBuf,
    clean_lock: PathBuf,
    sync_lock_base: PathBuf,
    data_lock_base: PathBuf,
}

impl StoragePaths {
    pub fn new(layout: &DuckHome) -> Self {
        let root = layout.storage_dir().to_path_buf();

        let package_dir = root.join(PKG_DIR_NAME);
        let venv_dir = root.join(VENVS_DIR_NAME);

        let lock_base = root.join(LOCKS_DIRECTORY_NAME);
        let clean_lock = lock_base.join(CLEAN_LOCK_FILENAME);
        let sync_lock_base = lock_base.join(VENV_SYNC_LOCK_FILENAME);
        let data_lock_base = lock_base.join(VENV_DATA_LOCK_FILENAME);
        Self {
            package_dir,
            venv_dir,
            clean_lock,
            sync_lock_base,
            data_lock_base,
        }
    }

    pub fn pkg_dir(&self, package: &PackageId) -> PathBuf {
        self.package_dir.join(package.storage_name())
    }

    pub fn clean_lock(&self) -> &Path {
        &self.clean_lock
    }

    pub fn sync_lock(&self, venv_id: StrId) -> PathBuf {
        self.sync_lock_base.join(venv_id)
    }

    pub fn data_lock(&self, venv_id: StrId) -> PathBuf {
        self.data_lock_base.join(venv_id)
    }

    pub fn venv_dir(&self, venv_id: StrId) -> PathBuf {
        self.venv_dir.join(venv_id)
    }

    pub fn vevn_metadata(&self, venv_id: StrId) -> PathBuf {
        self.venv_dir(venv_id).join(METADATA_FILENAME)
    }

    pub fn vevn_backup_metadata(&self, venv_id: StrId) -> PathBuf {
        self.venv_dir(venv_id).join(BACKUP_METADATA_FILENAME)
    }

    /// Returns an iterator over all packages in the storage.
    /// The yielded packages need not be correct (may be missing checksum).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_pkgs(&self) -> QuackResult<impl Iterator<Item = io::Result<DirEntry>>> {
        let dir = self.package_dir.as_path();
        if !dir.is_dir() {
            Ok(None.into_iter().flatten())
        } else {
            dir.read_dir()
                .map_err(Into::into)
                .map(Some)
                .map(|x| x.into_iter().flatten())
        }
    }

    /// Returns an iterator over all venvs in the storage.
    /// The yielded venvs need not be correct (may have invalid data).
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_vens(&self) -> QuackResult<impl Iterator<Item = io::Result<DirEntry>>> {
        let dir = self.venv_dir.as_path();
        if !dir.is_dir() {
            Ok(None.into_iter().flatten())
        } else {
            dir.read_dir()
                .map_err(Into::into)
                .map(Some)
                .map(|x| x.into_iter().flatten())
        }
    }

    /// Returns an iterator over all sync locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_sync_locks(&self) -> QuackResult<impl Iterator<Item = io::Result<DirEntry>>> {
        let dir = self.sync_lock_base.as_path();
        if !dir.is_dir() {
            Ok(None.into_iter().flatten())
        } else {
            dir.read_dir()
                .map_err(Into::into)
                .map(Some)
                .map(|x| x.into_iter().flatten())
        }
    }

    /// Returns an iterator over all data locks in the storage.
    /// It is not guaranteed that during iteration, the yielded paths
    /// still exist and there are no guarantees on paths that appeared during an iteration.
    pub fn iter_data_locks(&self) -> QuackResult<impl Iterator<Item = io::Result<DirEntry>>> {
        let dir = self.data_lock_base.as_path();
        if !dir.is_dir() {
            Ok(None.into_iter().flatten())
        } else {
            dir.read_dir()
                .map_err(Into::into)
                .map(Some)
                .map(|x| x.into_iter().flatten())
        }
    }

    pub fn is_package_stored(&self, id: &PackageId) -> bool {
        if id.is_local() {
            return false;
        }
        let dir = self.pkg_dir(id);
        dir.is_dir() && dir.join(OK_FILENAME).exists()
    }

    pub fn mark_as_stored(&self, id: &PackageId) -> QuackResult<()> {
        if id.is_local() {
            qp_bail_internal!("attempting to store a local package")
        }
        let dir = self.pkg_dir(id);
        dir.join(OK_FILENAME).touch()?;
        Ok(())
    }
}
