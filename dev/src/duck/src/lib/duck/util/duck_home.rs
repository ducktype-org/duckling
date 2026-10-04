//! This file represents a layout of global Duck home directory.
//! <Duck home root>
//! ├── cache
//! │   ├── downloads/ <directory for fetcher downloads>
//! │   ├── artifacts/ <directory for fetcher artifacts used for publishing packages>
//! │   ├── fetcher.lock <file>
//! │   └── metadata_db.sqlite <file with fetcher metadata cache>
//! ├── config.yaml <user config file>
//! ├── global_venv/ <root of the global shared virtual environment>
//! └── storage/ <root of the storage internal files>

use std::fs::{File, OpenOptions};
use std::io::Write;
use std::path::PathBuf;
use std::{fmt, io};

use tracing::{debug, error};

use crate::quackpack::core::PackageLoader;
use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult, QuackResultContext};

/// Implementation of the above layout
// We keep all of the paths, because then we don't have to do any allocations later.
pub struct DuckHome {
    root: FileLockManager,
}

impl fmt::Debug for DuckHome {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("DuckHome")
            .field("root", &self.root.not_locked_path())
            .finish_non_exhaustive()
    }
}

impl DuckHome {
    /// Create a new [`DuckHome`] rooted at `root`.
    pub fn new(root: PathBuf) -> Self {
        debug!(?root, "duck home");
        Self {
            root: FileLockManager::new(root),
        }
    }

    /// Get the [`PathBuf`] to the user config.
    pub fn user_config(&self) -> PathBuf {
        self.root.not_locked_path().join("config.yaml")
    }

    /// Get the [`FileLockManager`] rooted at the storage's root.
    pub fn storage(&self) -> FileLockManager {
        self.root.join("storage")
    }

    /// Get the [`FileLockManager`] rooted at the global venv root.
    pub fn global_venv(&self) -> FileLockManager {
        self.root.join("global_venv")
    }

    /// Get the [`FileLockManager`] rooted at the cache root.
    pub fn cache(&self) -> FileLockManager {
        self.root.join("cache")
    }

    /// Get the [`FileLockManager`] rooted at the cache artifacts root.
    pub fn artifacts(&self) -> FileLockManager {
        self.cache().join("artifacts")
    }

    /// Get the [`FileLockManager`] rooted at the cache downloads root.
    pub fn downloads(&self) -> FileLockManager {
        self.cache().join("downloads")
    }

    /// Exclusively lock the fetcher's lockfile.
    pub fn open_fetcher_lockfile(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.cache().open_exclusive("fetcher.lock", ctx)
    }

    /// Get the [`PathBuf`] to metadata database file.
    ///
    /// We don't have to create this file, as SQLite will do this for us.
    pub fn get_metadata_db_path(&self) -> PathBuf {
        self.cache()
            .into_not_locked_path()
            .join("metadata_db.sqlite")
    }

    pub const GLOBAL_PACKAGE_NAME: &str = "__global__";

    /// Default minimal manifest for the global package.
    pub fn default_global_manifest() -> String {
        format!(
            "\
metadata:
  name: {}
  version: '0.1'
  authors: []",
            Self::GLOBAL_PACKAGE_NAME
        )
    }

    /// Assure that the global package root folder exists and there is a manifest in it.
    pub fn ensure_and_populate_global_dir(&self) -> QuackResult<FileLockManager> {
        struct UnlockOnDrop {
            file: File,
        }

        impl Drop for UnlockOnDrop {
            fn drop(&mut self) {
                if let Err(e) = self.file.unlock() {
                    error!(error = %e, "failed to unlock the global manifest file");
                }
            }
        }
        let global_pkg_dir = self.global_venv();
        global_pkg_dir.mkdir()?;
        let manifest_path = global_pkg_dir
            .join(PackageLoader::MANIFEST_NAME)
            .into_not_locked_path();
        let file = {
            let mut opts = OpenOptions::new();
            opts.create_new(true).write(true);
            match opts.open(&manifest_path) {
                Ok(file) => Ok(file),
                Err(e) => {
                    // Global manifest already exists.
                    if e.kind() == io::ErrorKind::AlreadyExists {
                        return Ok(global_pkg_dir);
                    }
                    Err(e).context(format!("failed to create `{}`", manifest_path.display()))
                }
            }
        }?;
        file.lock()
            .with_context(|| format!("failed to lock the `{}`", manifest_path.display()))?;
        let mut guard = UnlockOnDrop { file };
        guard
            .file
            .write_all(Self::default_global_manifest().as_bytes())
            .with_context(|| format!("failed to write to the `{}`", manifest_path.display()))
            .context("failed to populate global venv")?;
        Ok(global_pkg_dir)
    }
}
