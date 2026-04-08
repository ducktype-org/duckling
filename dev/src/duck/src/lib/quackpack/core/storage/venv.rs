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
//! - [`fix_and_load`](Venv::fix_and_load): transforms state of a venv into the canonical form. Loads the state
//!   in the process (we do not provide separate fix and load, as checking validity
//!   of a state requires reading the state, so we just return that result)
//! - [`save_to`](Venv::save_to): requires that the state is in canonical form, transforms the state
//!   into a new canonical form with a new current state set to the provided.
//!
//! The format of the virtual environment file is:
//!
//! - JSON_DATA representing [`Venv`] object,
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
//! Instead we build higher level operations using [`copy_file_to`](PathOpsExt::copy_file_to) function,
//! which copies contents of a file and executes `fsync` on it.
//!
//! Note that when creating a new virtual environment, operating system might
//! break some invariants, by not flushing directory entries to the disk.
//! If directory `fsync` is supported, we use that, but in general, if system
//! failure happens during that window, we cannot guarantee the virtual environment
//! to exist after reboot even if it has been used before. The problem affects
//! however only relatively new virtual environments.

use std::{
    fmt,
    io::Write,
    path::{Path, PathBuf},
    time::SystemTime,
};

use serde::{Deserialize, Serialize};
use tracing::debug;

use crate::{
    QuackError, QuackResult, QuackResultContext,
    quackpack::core::storage::{
        freeze::{self, VenvFreeze},
        paths::Storage,
        venv_id::VenvId,
    },
    util::{
        hash,
        path_ops_ext::{MkdirOptions, PathOpsExt, ShouldBlock},
    },
};

#[derive(Debug)]
pub enum CorruptedVenvReason {
    MissingNewLine,
    InvalidChecksum { expected: String, actual: String },
    Other,
}

impl fmt::Display for CorruptedVenvReason {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::MissingNewLine => write!(f, "is missing a newline"),
            Self::InvalidChecksum { expected, actual } => write!(
                f,
                "checksums don't match: expected: {expected}, actual: {actual}"
            ),
            Self::Other => write!(f, "unknown reason"),
        }
    }
}

#[derive(Debug)]
pub struct CorruptedVenvError {
    pub path: PathBuf,
    pub reason: CorruptedVenvReason,
}

impl fmt::Display for CorruptedVenvError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "venv metadata located at `{}` is malformed, because: {}",
            self.path.display(),
            self.reason
        )
    }
}

impl std::error::Error for CorruptedVenvError {}

#[derive(Debug, Deserialize, Serialize)]
/// State of virtual environment in the storage. Stores the freeze for the given
/// virtual environment, copy of manifest's metadata, and additional info
/// required for storage functioning: last location and access info.
pub struct VenvData {
    freeze: freeze::VenvFreeze,
    is_ephemeral: bool,
    last_location: PathBuf,
    last_modification: SystemTime,
    last_access: SystemTime,
}

impl VenvData {
    /// Create a new [`VenvData`].
    pub fn new(
        freeze: freeze::VenvFreeze,
        is_ephemeral: bool,
        last_location: PathBuf,
        last_modification: SystemTime,
        last_access: SystemTime,
    ) -> Self {
        Self {
            freeze,
            is_ephemeral,
            last_location,
            last_modification,
            last_access,
        }
    }

    /// Load [`VenvData`] from the given path.
    fn load(path: &Path) -> QuackResult<Self> {
        debug!("loading venv data from `{}`", path.display());
        let content = path.read_to_string()?;
        let Some((data, checksum)) = content.rsplit_once("\n") else {
            debug!("missing newline for venv data at `{}`", path.display());
            return Err(CorruptedVenvError {
                reason: CorruptedVenvReason::MissingNewLine,
                path: path.to_path_buf(),
            }
            .into());
        };
        let current_hash = hash::sha256_string(data);
        if current_hash != checksum {
            debug!(
                "invalid checksum (current: `{}`, old: `{}`) for venv data at `{}`",
                current_hash,
                checksum,
                path.display()
            );
            return Err(CorruptedVenvError {
                reason: CorruptedVenvReason::InvalidChecksum {
                    expected: checksum.to_string(),
                    actual: current_hash,
                },
                path: path.to_path_buf(),
            }
            .into());
        }
        serde_json::from_str(data).map_err(|e| {
            debug!(
                "json error `{e}` while deserializing venv data at `{}`",
                path.display()
            );
            let err = QuackError::from(e);
            err.context(CorruptedVenvError {
                reason: CorruptedVenvReason::Other,
                path: path.to_path_buf(),
            })
        })
    }

    /// Save to the given path.
    fn save_to(&self, path: &Path) -> QuackResult<()> {
        let data = serde_json::to_string(self)?;
        let checksum = hash::sha256_string(&data);
        let mut file = path.touch()?;
        let full_data = format!("{data}\n{checksum}");
        debug!("for data `{data}` calculated checksum `{checksum}`");
        file.write_all(full_data.as_ref())?;
        file.flush()?;
        file.sync_data()?;
        Ok(())
    }

    /// Get a reference to the underlying [`VenvFreeze`].
    pub fn freeze(&self) -> &freeze::VenvFreeze {
        &self.freeze
    }

    /// Get a mutable reference to the underlying [`VenvFreeze`].
    pub fn freeze_mut(&mut self) -> &mut freeze::VenvFreeze {
        &mut self.freeze
    }

    /// Set the [`VenvFreeze`].
    pub fn set_freeze(&mut self, freeze: freeze::VenvFreeze) {
        self.freeze = freeze;
    }

    /// Whether this venv is ephemeral (temporary).
    pub fn is_ephemeral(&self) -> bool {
        self.is_ephemeral
    }

    /// Set the ephemerality (temporality) of this venv.
    pub fn set_ephemeral(&mut self, is_ephemeral: bool) {
        self.is_ephemeral = is_ephemeral;
    }

    /// Get the last known location of this venv.
    pub fn last_location(&self) -> &Path {
        &self.last_location
    }

    /// A mutable counterpart to the [`last_location`](Self::last_location).
    pub fn last_location_mut(&mut self) -> &mut PathBuf {
        &mut self.last_location
    }

    /// Set the last know location of this venv.
    pub fn set_last_location(&mut self, last_location: PathBuf) {
        self.last_location = last_location;
    }

    /// Get the last access time of this venv.
    pub fn last_access(&self) -> SystemTime {
        self.last_access
    }

    /// Set the last access time of this venv.
    pub fn set_last_access(&mut self, last_access: SystemTime) {
        self.last_access = last_access;
    }

    /// Get the last modification time of this venv.
    pub fn last_modification(&self) -> SystemTime {
        self.last_modification
    }

    /// Set the last modification time of this venv.
    pub fn set_last_modification(&mut self, last_modification: SystemTime) {
        self.last_modification = last_modification;
    }

    /// Update the last access and save this data to the disk.
    pub fn save_new_last_access(
        &mut self,
        last_access: SystemTime,
        path: &Path,
    ) -> QuackResult<()> {
        self.set_last_access(last_access);
        self.save_to(path)
    }
}

impl From<VenvData> for VenvFreeze {
    fn from(value: VenvData) -> Self {
        value.freeze
    }
}

impl From<Venv> for VenvData {
    fn from(value: Venv) -> Self {
        value.data
    }
}

impl From<Venv> for VenvFreeze {
    fn from(value: Venv) -> Self {
        value.data.freeze
    }
}

#[derive(Debug)]
/// A virtual environment.
pub struct Venv {
    id: VenvId,
    data: VenvData,
}

impl Venv {
    /// Create a new [`Venv`].
    pub fn new(id: VenvId, data: VenvData) -> Self {
        Self { id, data }
    }

    /// Get the ID of this venv.
    pub fn id(&self) -> VenvId {
        self.id
    }

    /// Set the ID of this venv.
    pub fn set_id(&mut self, id: VenvId) {
        self.id = id;
    }

    /// Get the underlying data of this venv.
    pub fn data(&self) -> &VenvData {
        &self.data
    }

    /// A mutable counterpart to the [`data`](Self::data).
    pub fn data_mut(&mut self) -> &mut VenvData {
        &mut self.data
    }

    /// Set the data of this venv.
    pub fn set_data(&mut self, data: VenvData) {
        self.data = data;
    }

    /// Convert the state of a virtual environment into canonical form and return its state.
    ///
    /// If neither the main nor backup file is valid, the environment directory is removed.
    pub fn fix_and_load(storage: &Storage, venv_id: VenvId) -> QuackResult<Option<Self>> {
        let _lock = storage
            .data_lock(venv_id)
            .lock(ShouldBlock::Yes)
            .with_context(|| {
                format!("failed to acquire an exclusive data lock for venv `{venv_id}`")
            })?;
        // NOTE: when external entity changes the storage disregarding the rules, we have
        // toctou here and an exception might be thrown later. We ignore that to keep sanity.
        if !storage.venv_dir(venv_id).is_dir() {
            debug!("storage for venv `{venv_id}` is not a directory");
            return Ok(None);
        }
        let path = storage.venv_metadata(venv_id);
        let backup_path = storage.venv_backup_metadata(venv_id);
        let existed = path.exists();
        let backup_existed = backup_path.exists();
        // if main file is valid, return state held in it
        if existed {
            let data = match VenvData::load(&path) {
                Ok(data) => Some(data),
                Err(e) => {
                    if let Some(err) = e.downcast_ref_in_chain::<CorruptedVenvError>() {
                        debug!("venv `{venv_id}` is corrupted: {err}");
                        None
                    } else {
                        return Err(e);
                    }
                }
            };
            if let Some(mut venv) = data {
                if let Err(e) = venv.save_new_last_access(SystemTime::now(), &path) {
                    debug!("failed to update last access time for venv `{venv_id}`: {e} ({e:?})");
                }
                return Ok(Some(Self::new(venv_id, venv)));
            }
        }

        // otherwise, the state is not canonical, and current state, if it exists,
        // is held in the backup file
        if backup_existed {
            let data = match VenvData::load(&path) {
                Ok(data) => Some(data),
                Err(e) => {
                    if let Some(err) = e.downcast_ref_in_chain::<CorruptedVenvError>() {
                        debug!("venv `{venv_id}` is corrupted: {err}");
                        None
                    } else {
                        return Err(e);
                    }
                }
            };
            if let Some(mut venv) = data {
                backup_path.copy_file_to(&path)?;
                if !existed {
                    storage.venv_dir(venv_id).try_fsync_dir()?;
                }
                if let Err(e) = venv.save_new_last_access(SystemTime::now(), &path) {
                    debug!("failed to update last access time for venv `{venv_id}`: {e} ({e:?})");
                }
                return Ok(Some(Self::new(venv_id, venv)));
            }
        }
        // both files are not valid, so the venv does not exist,
        // put it in the canonical form by deleting its directory
        storage.venv_dir(venv_id).rmtree()?;
        storage.venvs_base_dir().try_fsync_dir()?;
        Ok(None)
    }

    /// Save a new canonical state of the virtual environment to storage.
    ///
    /// Assumes that the current ``metadata`` file is valid. This is typically ensured
    /// by calling :func:`fix_and_load_venv` before.
    pub fn save_to(&self, storage: &Storage) -> QuackResult<()> {
        let _lock = storage
            .data_lock(self.id)
            .lock(ShouldBlock::Yes)
            .with_context(|| {
                format!(
                    "failed to acquire an exclusive data lock for venv `{}`",
                    self.id
                )
            })?;
        let path = storage.venv_metadata(self.id);
        let backup_path = storage.venv_backup_metadata(self.id);
        let existed = path.exists();
        let backup_existed = backup_path.exists();
        let parent = path
            .parent()
            .with_context_internal(|| format!("`{}` does not have a parent?", path.display()))?;
        if !parent.exists() {
            parent.mkdir(MkdirOptions::WithParents)?;
        }
        if existed {
            // move old current state to backup file, as when error occurs during
            // overwriting the main file, the invariants will be upkept.
            // (the backup file will be valid)
            path.copy_file_to(&backup_path)?;
            if !backup_existed {
                storage.venv_dir(self.id).try_fsync_dir()?;
            }
        }
        self.data.save_to(&path)?;
        if !existed {
            // also initialize the `.old` file, such that issues
            // relating to unavailable directory `fsync` are minimized
            path.copy_file_to(&backup_path)?;
            storage.venv_dir(self.id).try_fsync_dir()?;
        }
        Ok(())
    }
}
