use std::path::{Path, PathBuf};

use anyhow::anyhow;
use paste::item;
use rustvil::fs::{MkdirOptions, PathExt};

use crate::{
    DuckCtx, QuackResult, StrId,
    duck::util::terminal::Terminal,
    quackpack::util::paths::{
        artifacts_dir, download_dir, fetcher_lockfile, global_venv_dir, metadata_db, storage_dir,
    },
    static_str_id,
};

#[derive(Debug)]
/// An extension of DuckCtx with quackpack-specific functionalities.
pub struct QpCtx<'duck> {
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
            $name:ident, $toml_key:literal, $message_name:literal $(;)?
        )*
    ) => {
            $(
                pub fn $name(&self) -> QuackResult<PathBuf> {
                    // @TODO: #1555 (point 2) We may decide to only allow the configuration of the top level DUCK_HOME folder.
                    let path_buf = self
                        .inner
                        .toml_cfg()
                        .get_path($toml_key)?
                        .map(|path| path.to_path_buf())
                        .or_else(|| $name(self.inner.env()))
                        .ok_or_else(
                            || anyhow!("Could not decide where the {} should be placed", $message_name),
                        )?;
                    Ok(path_buf.as_path().expand_user()?.resolve()?)
                }
            )*
    };
}

/// Creates a function, which ensures that the file path to the appropriate quackpack-specific file is present in the filesystem.
macro_rules! file_ensurers {
    (
        $(
            $name:ident $(;)?
        )*
    ) => {
        item! {
            $(
                pub fn [<ensure_ $name>](&self) -> QuackResult<PathBuf> {
                    let path = self.$name()?;
                    path.touch()?;
                    Ok(path)
                }
            )*
        }
    };
}

/// Creates a function, which ensures that the directory to the appropriate quackpack-specific directory is present in the filesystem.
macro_rules! dir_ensurers {
    (
        $(
            $name:ident $(;)?
        )*
    ) => {
        item! {
            $(
                pub fn [<ensure_ $name>](&self) -> QuackResult<PathBuf> {
                    let path = self.$name()?;
                    path.mkdir(MkdirOptions::WithParents)?;
                    Ok(path)
                }
            )*
        }
    };
}

impl<'duck> QpCtx<'duck> {
    /// Creates QpCtx from DuckCtx.
    pub fn new(duck_ctx: &'duck DuckCtx) -> Self {
        Self { inner: duck_ctx }
    }

    /// Retrieves the underlying DuckCtx's console.
    pub fn console(&self) -> &Terminal {
        self.inner.console()
    }

    /// Retrieves the underlying DuckCtx's error console.
    pub fn error_console(&self) -> &Terminal {
        self.inner.error_console()
    }

    pub fn registry_url(&self) -> QuackResult<StrId> {
        Ok(self
            .inner
            .duck_cfg()
            .toml_config()
            .get_str("registry.url")?
            .map(StrId::from)
            // @TODO: #1548 Move this to the fetcher module
            .unwrap_or_else(|| static_str_id!("http://localhost:9001")))
    }

    pub fn cwd(&self) -> &Path {
        self.inner.cwd()
    }

    pub fn user_home(&self) -> &Path {
        self.inner.user_home()
    }

    pub fn duck_home(&self) -> &Path {
        self.inner.duck_home()
    }

    path_getters! {
        artifacts_dir, "cache.artifacts_dir", "artifacts directory";
        download_dir, "cache.download_dir", "download directory";
        storage_dir, "storage.dir", "quackpack storage directory";
        fetcher_lockfile, "cache.fetcher_lockfile", "fetcher lockfile";
        global_venv_dir, "global_venv", "global venv's manifest directory";
        metadata_db, "cache.metadata_db_path", "manifests metadata database";
    }

    file_ensurers! {
        fetcher_lockfile;
        metadata_db;
    }

    dir_ensurers! {
        artifacts_dir;
        download_dir;
        storage_dir;
        global_venv_dir;
    }
}
