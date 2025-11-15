use std::{
    fs::{File, create_dir_all},
    io::ErrorKind,
    path::{Path, PathBuf},
};

use anyhow::anyhow;
use paste::item;
use soft_canonicalize::soft_canonicalize;

use crate::{
    DuckCtx, QuackResult,
    duck::util::terminal::Terminal,
    quackpack::util::paths::{
        artifacts_dir, download_dir, fetcher_lockfile, global_venv_dir, metadata_db, storage_dir,
    },
};

/// An extension of DuckCtx with quackpack-specific functionalities.
pub struct QPCtx<'duck> {
    inner: &'duck DuckCtx,
}

/// Creates a function, which finds the path attributed to `toml_key` in the TOML configuration of the underlying DuckCtx.
/// If no such key was specified, falls back to the appropriate function from the `paths` module.
///
/// # Errors
/// - TOML parsing error related to the value attributed to `toml_key` occured.
/// - Appropriate function from the `paths` module returned an error when called.
/// - The retrieved path failed to soft_canonicalize.
macro_rules! path_getters {
    (
        $(
            $name:literal, $toml_key:literal, $message_name:literal
        );*
    ) => {
        item! {
            $(
                pub fn [<$name>](&self) -> QuackResult<PathBuf> {
                    soft_canonicalize(
                    self.inner.toml_cfg().get_path(stringify!($toml_key))?.or_else(|| [<$name>](self.inner.env()))
                        .ok_or_else(
                            || anyhow!("Could not decide where the {} should be placed", stringify!($message_name))
                        )?).map_err(|e| e.into())
                }
            )*
        }
    };
}

/// Creates a function, which ensures that the file path to the appropriate quackpack-specific file is present in the filesystem.
macro_rules! file_ensurers {
    (
        $(
            $name:ident
        );*
    ) => {
        item! {
            $(
                pub fn [<ensure_ $name>](&self) -> QuackResult<()> {
                    let path = self.$name()?;
                    self.ensure_file(&path)
                }
            )*
        }
    };
}

/// Creates a function, which ensures that the directory to the appropriate quackpack-specific directory is present in the filesystem.
macro_rules! dir_ensurers {
    (
        $(
            $name:ident
        );*
    ) => {
        item! {
            $(
                pub fn [<ensure_ $name>](&self) -> QuackResult<()> {
                    let path = self.$name()?;
                    self.ensure_dir(&path)
                }
            )*
        }
    };
}

impl<'duck> QPCtx<'duck> {
    /// Creates QPCtx from DuckCtx.
    pub fn new(duck_ctx: &'duck DuckCtx) -> Self {
        Self { inner: duck_ctx }
    }

    /// Retrieves the underlying DuckCtx's console.
    pub fn console(&self) -> &Terminal {
        self.inner.console()
    }

    /// Helper function, ensures that a given directory is present in the filesystem.
    fn ensure_dir(&self, path: &Path) -> QuackResult<()> {
        create_dir_all(path)?;
        Ok(())
    }

    /// Helper function, ensures that a given file is present in the filesystem.
    fn ensure_file(&self, path: &Path) -> QuackResult<()> {
        if let Some(parent) = path.parent() {
            self.ensure_dir(parent)?;
            // create() truncates the file, thus this work-around is needed.
            match File::create_new(path) {
                Ok(_) => Ok(()),
                Err(e) => {
                    if matches!(e.kind(), ErrorKind::AlreadyExists) {
                        Ok(())
                    } else {
                        Err(anyhow::Error::from(e))
                    }
                }
            }
        } else {
            Err(anyhow!("The provided path does not point to a file"))
        }
    }

    path_getters! {
        "artifacts_dir", "cache.artifacts_dir", "artifacts directory";
        "download_dir", "cache.download_dir", "download directory";
        "storage_dir", "storage.dir", "quackpack storage directory";
        "fetcher_lockfile", "cache.fetcher_lockfile", "fetcher lockfile";
        "global_venv_dir", "global_venv", "global venv's manifest directory";
        "metadata_db", "cache.metadata_db_path", "manifests metadata database"
    }

    file_ensurers! {
        fetcher_lockfile;
        metadata_db
    }

    dir_ensurers! {
        artifacts_dir;
        download_dir;
        storage_dir;
        global_venv_dir
    }
}
