use std::{
    fs::{File, create_dir_all},
    io::ErrorKind,
    path::{Path, PathBuf},
};

use anyhow::anyhow;
use paste::item;

use crate::{
    DuckCtx, QuackResult,
    quackpack::paths::{
        artifacts_dir, download_dir, fetcher_lockfile, global_venv_dir, metadata_db, storage_dir,
    },
    terminal::Terminal,
};

pub struct QPCtx<'duck> {
    inner: &'duck DuckCtx,
}

macro_rules! path_getters {
    (
        $(
            $name:literal, $key:literal, $message_name:literal
        );*
    ) => {
        item! {
            $(
                pub fn [<$name>](&self) -> QuackResult<PathBuf> {
                    if let Some(path) = self.inner.toml_cfg().get_path(stringify!($key))? {
                        path.canonicalize().map_err(|e| e.into())
                    }
                    else {
                        [<$name>](self.inner.env()).ok_or_else(
                            || anyhow!("Could not decide where the {} should be placed", stringify!($message_name))
                        )
                    }
                }
            )*
        }
    };
}

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
    pub fn new(duck_ctx: &'duck DuckCtx) -> Self {
        Self { inner: duck_ctx }
    }

    pub fn console(&self) -> &Terminal {
        self.inner.console()
    }

    fn ensure_dir(&self, path: &Path) -> QuackResult<()> {
        create_dir_all(path)?;
        Ok(())
    }

    fn ensure_file(&self, path: &Path) -> QuackResult<()> {
        if let Some(parent) = path.parent() {
            self.ensure_dir(parent)?;
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
