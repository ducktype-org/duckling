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
    path::Path,
};

use serde::{Deserialize, Serialize};

use crate::{
    QuackResult, StrId,
    quackpack::core::{FeatureName, Git, GitId, PackageId},
};

const BUFFER_SIZE: usize = 4096;

pub(super) trait PathExt {
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

#[derive(Debug, Serialize, Deserialize)]
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
