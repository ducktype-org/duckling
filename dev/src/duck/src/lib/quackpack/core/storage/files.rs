//! Provides operations on the virtual environment state file, which preserve
//! coherency of the storage. Also exposes simpler helpers for general
//! system-failure safe file operations, which are be used to build
//! up higher level operations.
//!
//! The state of virtual environment is changed with following invariants:
//!
//! 1. Each existing venv holds at least one of `metadata` or `metadata.old` files,
//!    each one of them internally holds a checksum that assures its validity.
//!    A file that exists on a disk and contains a correct checksum is considered valid.
//! 2. At any point (also during any of the lower level operations), for any
//!    existing venv, at least one of the files is valid. If both of them are,
//!    the version held in `metadata` is considered to be the current state.
//!    A venv with both files invalid is considered nonexistent.
//! 3. We never change the state of the venv from existing to nonexistent (with
//!    operations provided in this module, other methods like removing venv
//!    directory may cause such change). Also, after a new state has been saved,
//!    and becomes valid we do not regress to considering older state to be current.
//! 4. A state in which venv exists and `metadata` holds a current state, or venv does
//!    not exist and the venv directory is empty or does not exist will be named "canonical".
//!
//! There are two operations:
//!
//! - [`fix_and_load`]: transforms state of a venv into the canonical form. Loads the state
//!   in the process (we do not provide separate fix and load, as checking validity
//!   of a state requires reading the state, so we just return that result)
//! - [`save`]: requires that the state is in canonical form, transforms the state
//!   into a new canonical form with a new current state set to the provided.
//!
//! The format of the virtual environment file is:
//!
//! - JSON_DATA representing [`StorageVenv`] object,
//! - a sha256 hash of the preceding data in a new line, for content validity checking.
//!
//! Some considerations:
//!
//! It may seem that state coherency can be achieved in a simpler way, by creating
//! a new one, and moving some files around. Due to insufficient guarantees
//! from some operating systems, we try to minimize creating, moving and deleting files:
//! `os.fsync` for files is supported on most platforms, while `os.fsync`
//! for directories does not work for example on Windows, which makes it hard
//! to guarantee, which files will exist and where after system failure.
//! Instead we build higher level operations using [`transfer_file`] function,
//! which copies contents of a file and executes [`fsync`] on it.
//!
//! Note that when creating a new virtual environment, operating system might
//! break some invariants, by not flushing directory entries to the disk.
//! If directory [`fsync`] is supported, we use that, but in general, if system
//! failure happens during that window, we cannot guarantee the virtual environment
//! to exist after reboot even if it has been used before. The problem affects
//! however only relatively new virtual environments.

use std::{
    collections::HashMap,
    fs::OpenOptions,
    io::{self, Read, Write},
    path::{Path, PathBuf},
    time::SystemTime,
};

use rustvil::fs::{MkdirOptions, PathExt as _};
use serde::{Deserialize, Serialize};
use tracing::debug;

use crate::{
    QuackResult, QuackResultContext, StrId,
    quackpack::{
        core::{FeatureName, Git, storage::paths::StoragePaths},
        schemas::registry,
    },
    util_common::hash,
};

use super::package_id::{GitId, PackageId};

const BUFFER_SIZE: usize = 4096;

pub trait PathExt {
    /// Copy the contents of one file into another and synchronize the result to disk.
    fn transfer_file_to<P: AsRef<Path>>(&self, to: P) -> QuackResult<()>;

    /// Ensure that directory-level changes (creation, deletion, renaming) are persisted to disk.
    ///
    /// This is a best-effort operation. If unsupported, it will be silently skipped.
    fn try_fsync_dir(&self) -> QuackResult<()>;
}

impl PathExt for Path {
    fn transfer_file_to<P: AsRef<Path>>(&self, to: P) -> QuackResult<()> {
        let mut source = {
            let mut opts = OpenOptions::new();
            opts.read(true).open(self)
        }?;
        let mut target = {
            let mut opts = OpenOptions::new();
            opts.write(true).create(true).open(to)
        }?;
        let mut buffer = [0; BUFFER_SIZE];
        loop {
            let n = source.read(&mut buffer)?;
            if n == 0 {
                break;
            }
            target.write_all(&buffer[..n])?;
        }
        target.flush()?;
        target.sync_all()?;
        Ok(())
    }

    fn try_fsync_dir(&self) -> QuackResult<()> {
        let dir = {
            let mut opts = OpenOptions::new();
            opts.read(true).open(self)
        }?;
        match dir.sync_all() {
            Ok(_) => Ok(()),
            Err(e) if matches!(e.kind(), io::ErrorKind::Unsupported) => Ok(()),
            Err(e) => Err(e.into()),
        }
    }
}

#[derive(Debug)]
pub struct CorruptedFileError {}

#[derive(Debug, Serialize, Deserialize, PartialEq, Eq)]
/// Information required to build a package in a given dependencies realization.
pub struct PackageFreeze {
    pub dependencies: HashMap<StrId, PackageId>,
    pub used_flags: Vec<FeatureName>,
}

#[derive(Debug, Deserialize, Serialize)]
struct Dependency {
    id: PackageId,
    data: PackageFreeze,
}

#[derive(Debug, Deserialize, Serialize)]
struct GitFetchCacheEntry {
    source: Git,
    result: GitId,
}

#[derive(Debug, Deserialize, Serialize, PartialEq, Eq)]
/// Realization of requirements stored in virtual environment manifest.
/// Contains all the information required to build and run code using the given virtual environment.
pub struct VenvFreeze {
    pub direct_dependencies: HashMap<StrId, PackageId>,
    pub dependencies: HashMap<PackageId, PackageFreeze>,
    pub git_fetch_cache: HashMap<Git, GitId>,
}

#[derive(Debug, Deserialize, Serialize)]
/// State of virtual environment in the storage. Stores the freeze for the given
/// virtual environment, copy of manifest's metadata, and additional info
/// required for storage functioning: last location and access info.
pub struct StorageVenv {
    pub freeze: VenvFreeze,
    pub original_schema: registry::Manifest,
    pub is_ephemeral: bool,
    pub last_location: PathBuf,
    pub last_modification: SystemTime,
    pub last_access: SystemTime,
}

impl StorageVenv {
    pub fn load(path: &Path) -> QuackResult<Result<Self, CorruptedFileError>> {
        // @TODO: #1353 EnableInterrupts
        let content = path.read_to_string()?;
        let Some((data, checksum)) = content.rsplit_once("\n") else {
            return Ok(Err(CorruptedFileError {}));
        };
        let current_hash = hash::sha256_string(data);
        if current_hash != checksum {
            return Ok(Err(CorruptedFileError {}));
        }
        Ok(serde_json::from_str(data).map_err(|_| CorruptedFileError {}))
    }

    pub fn save(&self, path: &Path) -> QuackResult<()> {
        let data = serde_json::to_string(self)?;
        let checksum = hash::sha256_string(&data);
        let mut file = path.touch()?;
        // @TODO: #1353 EnableInterrupts
        file.write_all(format!("{data}\n{checksum}").as_ref())?;
        file.flush()?;
        file.sync_all()?;
        Ok(())
    }
}

/// Convert the state of a virtual environment into canonical form and return its state.
///
/// If neither the main nor backup file is valid, the environment directory is removed.
pub fn fix_and_load_venv(
    storage: &StoragePaths,
    venv_id: StrId,
) -> QuackResult<Option<StorageVenv>> {
    // NOTE: when external entity changes the storage disregarding the rules, we have
    // toctou here and an exception might be thrown later. We ignore that to keep sanity.
    if !storage.venv_dir(venv_id).is_dir() {
        debug!("storage for venv `{venv_id}` is not a directory");
        return Ok(None);
    }
    let path = storage.vevn_metadata(venv_id);
    let backup_path = storage.vevn_backup_metadata(venv_id);
    let existed = path.exists();
    let backup_existed = backup_path.exists();
    // if main file is valid, return state held in it
    if existed {
        let data = StorageVenv::load(&path)?;
        if let Ok(venv) = data {
            return Ok(Some(venv));
        }
    }

    // otherwise, the state is not canonical, and current state, if it exists,
    // is held in the backup file
    if backup_existed {
        let data = StorageVenv::load(&path)?;
        if let Ok(venv) = data {
            // @TODO: #1353 EnableInterrupts
            backup_path.transfer_file_to(&path)?;
            if !existed {
                // @TODO: #1353 EnableInterrupts
                storage.venv_dir(venv_id).try_fsync_dir()?;
            }
            return Ok(Some(venv));
        }
    }
    // both files are not valid, so the venv does not exist,
    // put it in the canonical form by deleting its directory
    // @TODO: #1353 EnableInterrupts
    storage.venv_dir(venv_id).rmtree()?;
    Ok(None)
}

/// Save a new canonical state of the virtual environment to storage.
///
/// Assumes that the current ``metadata`` file is valid. This is typically ensured
/// by calling :func:`fix_and_load_venv` before.
pub fn save_venv(storage: &StoragePaths, venv_id: StrId, venv: &StorageVenv) -> QuackResult<()> {
    let path = storage.vevn_metadata(venv_id);
    let backup_path = storage.vevn_backup_metadata(venv_id);
    let existed = path.exists();
    let backup_existed = backup_path.exists();
    let parent = path
        .parent()
        .with_context_internal(|| format!("`{}` does not have a parent?", path.display()))?;
    if !parent.exists() {
        // @TODO: #1353 EnableInterrupts
        parent.mkdir(MkdirOptions::WithParents)?;
    }
    if existed {
        // move old current state to backup file, as when error occurs during
        // overwriting the main file, the invariants will be upkept.
        // (the backup file will be valid)
        // @TODO: #1353 EnableInterrupts
        path.transfer_file_to(&backup_path)?;
        if !backup_existed {
            // @TODO: #1353 EnableInterrupts
            storage.venv_dir(venv_id).try_fsync_dir()?;
        }
    }
    venv.save(&path)?;
    if !existed {
        // also initialize the `.old` file, such that issues
        // relating to unavailable directory `fsync` are minimized
        // @TODO: #1353 EnableInterrupts
        path.transfer_file_to(&backup_path)?;
        storage.venv_dir(venv_id).try_fsync_dir()?;
    }
    Ok(())
}
