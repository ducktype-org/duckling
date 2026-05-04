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

use std::fs::OpenOptions;
use std::io::Write;
use std::path::PathBuf;
use std::{fmt, io};

use tracing::debug;

use crate::quackpack::core::PackageLoader;
use crate::util::filesystem::{Filesystem, LockedFile};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext};

/// Implementation of the above layout
// We keep all of the paths, because then we don't have to do any allocations later.
pub struct DuckHome {
    root: Filesystem,
}

impl fmt::Debug for DuckHome {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("DuckHome")
            .field("root", &self.root)
            .finish_non_exhaustive()
    }
}

impl DuckHome {
    /// Create a new [`DuckHome`] rooted at `root`.
    pub fn new(root: PathBuf) -> Self {
        debug!(?root, "duck home root");
        Self {
            root: Filesystem::new(root),
        }
    }

    pub fn user_config(&self) -> PathBuf {
        self.root.not_locked_path().join("config.yaml")
    }

    pub fn storage(&self) -> Filesystem {
        self.root.join("storage")
    }

    pub fn global_venv(&self) -> Filesystem {
        self.root.join("global_venv")
    }

    pub fn cache(&self) -> Filesystem {
        self.root.join("cache")
    }

    pub fn artifacts(&self) -> Filesystem {
        self.cache().join("artifacts")
    }

    pub fn downloads(&self) -> Filesystem {
        self.cache().join("downloads")
    }

    pub fn open_fetcher_lockfile(&self, ctx: &DuckContext) -> QuackResult<LockedFile> {
        self.cache().open_exclusive("fetcher.lock", ctx)
    }

    pub fn ensure_metadata_db(&self) -> QuackResult<PathBuf> {
        let mut root = self.cache().into_not_locked_path();
        root.push("metadata_db.sqlite");
        root.touch()?;
        Ok(root)
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
    pub fn ensure_and_populate_global_dir(&self) -> QuackResult<Filesystem> {
        let global_pkg_dir = self.global_venv();
        global_pkg_dir.mkdir()?;
        let manifest_path = global_pkg_dir
            .join(PackageLoader::MANIFEST_NAME)
            .into_not_locked_path();
        let mut file = {
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
        file.write_all(Self::default_global_manifest().as_bytes())
            .with_context(|| format!("failed to write to the `{}`", manifest_path.display()))
            .context("failed to populate global venv")?;
        Ok(global_pkg_dir)
    }
}
