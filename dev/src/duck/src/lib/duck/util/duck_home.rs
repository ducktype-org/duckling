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

use crate::{
    QuackResult,
    quackpack::core::PackageLoader,
    util::{env::Env, path_ops_ext::PathOpsExt},
};
use std::path::{Path, PathBuf};
use tracing::debug;

macro_rules! getter {
    (
        MemberName: $name:ident,
        Description: $desc:literal,
        EnsureFunction: $fn:ident $(,)?
    ) => {
        /// Get the path for the
        #[doc = $desc]
        /// Note that it may not exist on the disk
        pub fn $name(&self) -> &Path {
            &self.$name
        }
    };
}

macro_rules! ensure_file {
    (
        MemberName: $name:ident,
        Description: $desc:literal,
        EnsureFunction: $fn:ident $(,)?
    ) => {
        /// Ensure that the file
        #[doc = $desc]
        /// exists on the disk
        pub fn $fn(&self) -> QuackResult<&Path> {
            use $crate::util::path_ops_ext::PathOpsExt;
            let file = self.$name();
            let _ = file.touch()?;
            Ok(file)
        }
    };
}

macro_rules! ensure_dir {
    (
        MemberName: $name:ident,
        Description: $desc:literal,
        EnsureFunction: $fn:ident $(,)?
    ) => {
        /// Ensure that the directory
        #[doc = $desc]
        /// exists on the disk
        pub fn $fn(&self) -> QuackResult<&Path> {
            use $crate::util::path_ops_ext::{MkdirOptions, PathOpsExt};
            let file = self.$name();
            let _ = file.mkdir(MkdirOptions::WithParents)?;
            Ok(file)
        }
    };
}

macro_rules! call_on_files {
    ($callback:ident) => {
        $callback! {
            MemberName: fetcher_lockfile,
            Description: "fetcher lockfile",
            EnsureFunction: ensure_fetcher_lockfile,
        }
        $callback! {
            MemberName: metadata_db,
            Description: "fetcher metadata database",
            EnsureFunction: ensure_metadata_db,
        }
    };

    ($callback:ident INCLUDE_USER_CONFIG) => {
        $callback! {
            MemberName: user_config,
            Description: "user config",
            EnsureFunction: ensure_user_config,
        }

        $callback! {
            MemberName: fetcher_lockfile,
            Description: "fetcher lockfile",
            EnsureFunction: ensure_fetcher_lockfile,
        }
        $callback! {
            MemberName: metadata_db,
            Description: "fetcher metadata database",
            EnsureFunction: ensure_metadata_db,
        }
    };
}

macro_rules! call_on_dirs {
    ($callback:ident) => {
        $callback! {
            MemberName: root,
            Description: "duck home root directory",
            EnsureFunction: ensure_root,
        }

        $callback! {
            MemberName: downloads_dir,
            Description: "fetcher downloads directory",
            EnsureFunction: ensure_downloads_dir,
        }

        $callback! {
            MemberName: cache_dir,
            Description: "cache directory",
            EnsureFunction: ensure_cache_dir,
        }

        $callback! {
            MemberName: artifacts_dir,
            Description: "fetcher artifacts directory",
            EnsureFunction: ensure_artifacts_dir,
        }

        $callback! {
            MemberName: storage_dir,
            Description: "storage directory",
            EnsureFunction: ensure_storage_dir,
        }
        $callback! {
            MemberName: global_venv_dir,
            Description: "global venv directory",
            EnsureFunction: ensure_global_dir,
        }
    };
}

#[derive(Debug)]
/// Implementation of the above layout
// We keep all of the paths, because then we don't have to do any allocations later.
pub struct DuckHome {
    root: PathBuf,
    cache_dir: PathBuf,
    downloads_dir: PathBuf,
    artifacts_dir: PathBuf,
    fetcher_lockfile: PathBuf,
    metadata_db: PathBuf,
    user_config: PathBuf,
    storage_dir: PathBuf,
    global_venv_dir: PathBuf,
}

impl DuckHome {
    /// Create a new [`DuckHome`] rooted at `root`, and using environmental variables from [`Env`].
    pub fn new(root: PathBuf, env: &Env) -> Self {
        fn get_key_with_fallback(
            env: &Env,
            path: &'static str,
            fallback: impl FnOnce() -> PathBuf,
        ) -> PathBuf {
            env.get_os(path).map(PathBuf::from).unwrap_or_else(fallback)
        }
        // @TODO: #1671 Right now these are hardcoded. Idea is, that they can be set in config, but also as an environmental variable.
        //  Let's take a cache directory as a prime example. In config it can be set in:
        //  ```yaml
        //  cache:
        //    dir: path
        //  ```
        //  It's *path* is `cache.dir`. Then we would process this path to get DUCK_CACHE_DIR: an environmental variable name corresponding to this config value.
        let cache_dir = get_key_with_fallback(env, "DUCK_CACHE_DIR", || root.join("cache"));
        let downloads_dir = get_key_with_fallback(env, "DUCK_CACHE_DOWNLOADS_DIR", || {
            cache_dir.join("downloads")
        });
        let artifacts_dir = get_key_with_fallback(env, "DUCK_CACHE_ARTIFACTS_DIR", || {
            cache_dir.join("artifacts")
        });
        let fetcher_lockfile = get_key_with_fallback(env, "DUCK_CACHE_FETCHER_LOCKFILE", || {
            cache_dir.join("fetcher.lock")
        });
        let metadata_db = get_key_with_fallback(env, "DUCK_CACHE_METADATA_DB", || {
            cache_dir.join("metadata_db.sqlite")
        });
        let user_config = get_key_with_fallback(env, "DUCK_CONFIG", || root.join("config.yaml"));

        let storage_dir = get_key_with_fallback(env, "DUCK_STORAGE_DIR", || root.join("storage"));

        let global_venv_dir =
            get_key_with_fallback(env, "DUCK_STORAGE_GLOBAL_VENV", || root.join("global_venv"));
        let duck_home = Self {
            root,
            cache_dir,
            downloads_dir,
            artifacts_dir,
            fetcher_lockfile,
            metadata_db,
            user_config,
            storage_dir,
            global_venv_dir,
        };
        debug!("duck home layout is `{duck_home:?}`");
        duck_home
    }

    call_on_dirs! {
        getter
    }

    call_on_files! {
        getter INCLUDE_USER_CONFIG
    }

    call_on_dirs! {
        ensure_dir
    }

    call_on_files! {
        ensure_file
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
    pub fn ensure_and_populate_global_dir(&self) -> QuackResult<&Path> {
        let global_pkg_dir = self.ensure_global_dir()?;
        let manifest_path = global_pkg_dir.join(PackageLoader::MANIFEST_NAME);
        if !manifest_path.exists() {
            manifest_path.write(Self::default_global_manifest().as_bytes())?;
        }
        Ok(global_pkg_dir)
    }
}
