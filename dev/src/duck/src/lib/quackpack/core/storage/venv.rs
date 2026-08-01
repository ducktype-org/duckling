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

use std::fmt;
use std::io::Write;
use std::path::{Path, PathBuf};

use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use tracing::{debug, error, info, trace};

use crate::quackpack::core::storage::freeze::{self, VenvFreeze};
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv_id::VenvId;
use crate::util::hash;
use crate::util::path_ops_ext::{MkdirOptions, PathOpsExt};
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext};

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

#[derive(Debug, Deserialize, Serialize, Clone, PartialEq, Eq, Hash)]
#[serde(rename_all = "kebab-case")]
/// State of virtual environment in the storage. Stores the freeze for the given
/// virtual environment, copy of manifest's metadata, and additional info
/// required for storage functioning: last location and access info.
pub struct VenvData {
    freeze: freeze::VenvFreeze,
    is_ephemeral: bool,
    last_known_location: PathBuf,
    last_synchronization: DateTime<Utc>,
    last_access: DateTime<Utc>,
}

impl VenvData {
    /// Create a new [`VenvData`].
    pub fn new(
        freeze: freeze::VenvFreeze,
        is_ephemeral: bool,
        last_known_location: PathBuf,
        last_synchronization: DateTime<Utc>,
        last_access: DateTime<Utc>,
    ) -> Self {
        Self {
            freeze,
            is_ephemeral,
            last_known_location,
            last_synchronization,
            last_access,
        }
    }

    /// Load [`VenvData`] from the given path.
    #[tracing::instrument]
    fn load(path: &Path) -> QuackResult<Self> {
        trace!("loading venv data");
        let content = path.read_to_string()?;
        let Some((data, found_checksum)) = content.rsplit_once("\n") else {
            error!("missing a newline");
            return Err(CorruptedVenvError {
                reason: CorruptedVenvReason::MissingNewLine,
                path: path.to_path_buf(),
            }
            .into());
        };
        let expected_checksum = hash::sha256_string(data);
        if expected_checksum != found_checksum {
            error!(
                %expected_checksum,
                %found_checksum,
                "invalid checksum",
            );
            return Err(CorruptedVenvError {
                reason: CorruptedVenvReason::InvalidChecksum {
                    actual: found_checksum.to_string(),
                    expected: expected_checksum,
                },
                path: path.to_path_buf(),
            }
            .into());
        }
        serde_json::from_str(data).map_err(|e| {
            error!(error = %e, "json error");
            let err = QuackError::from(e);
            err.context(CorruptedVenvError {
                reason: CorruptedVenvReason::Other,
                path: path.to_path_buf(),
            })
        })
    }

    /// Save to the given path.
    #[tracing::instrument(skip(self))]
    fn save_to(&self, path: &Path) -> QuackResult<()> {
        trace!("saving venv data");
        let data = serde_json::to_string(self)?;
        let checksum = hash::sha256_string(&data);
        let mut file = path.touch()?;
        file.set_len(0)
            .with_context(|| format!("failed to truncate `{}`", path.display()))?;
        let full_data = format!("{data}\n{checksum}");
        debug!(%checksum);
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
    /// This is either root of the package for packages or path to the script for scripts.
    pub fn last_known_location(&self) -> &Path {
        &self.last_known_location
    }

    /// A mutable counterpart to the [`last_known_location`](Self::last_known_location).
    pub fn last_known_location_mut(&mut self) -> &mut PathBuf {
        &mut self.last_known_location
    }

    /// Set the last know location of this venv.
    pub fn set_last_known_location(&mut self, last_known_location: PathBuf) {
        self.last_known_location = last_known_location;
    }

    /// Get the last access time of this venv.
    pub fn last_access(&self) -> DateTime<Utc> {
        self.last_access
    }

    /// Set the last access time of this venv.
    pub fn set_last_access(&mut self, last_access: DateTime<Utc>) {
        self.last_access = last_access;
    }

    /// Get the last modification time of this venv.
    pub fn last_synchronization(&self) -> DateTime<Utc> {
        self.last_synchronization
    }

    /// Set the last modification time of this venv.
    pub fn set_last_synchronization(&mut self, last_synchronization: DateTime<Utc>) {
        self.last_synchronization = last_synchronization;
    }

    /// Update the last access and save this data to the disk.
    pub fn save_new_last_access(
        &mut self,
        last_access: DateTime<Utc>,
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

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
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
    #[tracing::instrument(skip_all)]
    pub fn fix_and_load(
        storage: &Storage,
        venv_id: VenvId,
        ctx: &DuckContext,
    ) -> QuackResult<Option<Self>> {
        trace!(id = %venv_id, "loading venv");
        let _lock = storage
            .data_locks()
            .open_exclusive(venv_id, ctx)
            .with_context(|| {
                format!(
                    "failed to acquire an exclusive data lock for venv `{}`",
                    venv_id
                )
            })?;
        Self::fix_and_load_with_lock_held(storage, venv_id).map(|r| r.map(|(venv, _)| venv))
    }

    /// Convert the state of a virtual environment into canonical form and return its state.
    /// Since loading the venv overrides the last_access value,
    /// this function returns the previous last_access value as well as the loaded venv.
    ///
    /// If neither the main nor backup file is valid, the environment directory is removed.
    #[tracing::instrument(skip_all, fields(id = %venv_id))]
    pub fn fix_and_load_with_last_access(
        storage: &Storage,
        venv_id: VenvId,
        ctx: &DuckContext,
    ) -> QuackResult<Option<(Self, DateTime<Utc>)>> {
        trace!("loading venv");
        let _lock = storage
            .data_locks()
            .open_exclusive(venv_id, ctx)
            .with_context(|| {
                format!(
                    "failed to acquire an exclusive data lock for venv `{}`",
                    venv_id
                )
            })?;
        Self::fix_and_load_with_lock_held(storage, venv_id)
    }

    /// Helper for [`fix_and_load`](Self::fix_and_load) and [`fix_and_load_with_last_access`](Self::fix_and_load_with_last_access).
    #[tracing::instrument(skip(storage))]
    fn fix_and_load_with_lock_held(
        storage: &Storage,
        venv_id: VenvId,
    ) -> QuackResult<Option<(Self, DateTime<Utc>)>> {
        // NOTE: when external entity changes the storage disregarding the rules, we have
        // toctou here and an exception might be thrown later. We ignore that to keep sanity.
        if !storage.venv_dir(venv_id).is_dir() {
            debug!("not a directory");
            return Ok(None);
        }
        let metadata = storage.venv_metadata(venv_id);
        let backup_metadata = storage.venv_backup_metadata(venv_id);
        // if main file is valid, return state held in it
        if metadata.exists() {
            let data = match VenvData::load(&metadata) {
                Ok(data) => Some(data),
                Err(e) => {
                    if let Some(err) = e.downcast_ref_in_chain::<CorruptedVenvError>() {
                        error!(error = %err, "corrupted");
                        None
                    } else {
                        return Err(e);
                    }
                }
            };
            if let Some(venv) = data {
                let mut this = Self::new(venv_id, venv);
                let previous_now = this.data().last_access();
                this.data_mut().set_last_access(Utc::now());
                if let Err(e) = this.save_to_with_lock_held(storage) {
                    error!(error = %e, "failed to update last atime");
                    info!(?previous_now, "restoring previous atime");
                    this.data_mut().set_last_access(previous_now);
                }
                return Ok(Some((this, previous_now)));
            }
        }

        // otherwise, the state is not canonical, and current state, if it exists,
        // is held in the backup file
        if backup_metadata.exists() {
            let data = match VenvData::load(&backup_metadata) {
                Ok(data) => Some(data),
                Err(e) => {
                    if let Some(err) = e.downcast_ref_in_chain::<CorruptedVenvError>() {
                        error!(error = %err, "corrupted");
                        None
                    } else {
                        return Err(e);
                    }
                }
            };
            if let Some(venv) = data {
                backup_metadata.copy_to(metadata)?;
                let mut this = Self::new(venv_id, venv);
                let previous_now = this.data().last_access();
                this.data_mut().set_last_access(Utc::now());
                if let Err(e) = this.save_to_with_lock_held(storage) {
                    error!(error = %e, "failed to update last atime");
                    info!(?previous_now, "restoring previous atime");
                    this.data_mut().set_last_access(previous_now);
                }
                return Ok(Some((this, previous_now)));
            }
        }
        // both files are not valid, so the venv does not exist,
        // put it in the canonical form by deleting its directory
        storage.venv_dir(venv_id).rmtree()?;
        storage.venvs_root_dir().try_fsync_dir()?;
        Ok(None)
    }

    /// Save a new canonical state of the virtual environment to storage.
    ///
    /// Assumes that the current `metadata` file is valid. This is typically ensured
    /// by calling [`fix_and_load`](Self::fix_and_load) before.
    #[tracing::instrument(skip_all, fields(id = %self.id))]
    pub fn save_to(&self, storage: &Storage, ctx: &DuckContext) -> QuackResult<()> {
        trace!("saving venv");
        let _lock = storage
            .data_locks()
            .open_exclusive(self.id, ctx)
            .with_context(|| {
                format!(
                    "failed to acquire an exclusive data lock for venv `{}`",
                    self.id
                )
            })?;
        self.save_to_with_lock_held(storage)
    }

    /// Helper for [`save_to`](Self::save_to).
    #[tracing::instrument(skip_all, fields(id = ?self.id))]
    fn save_to_with_lock_held(&self, storage: &Storage) -> QuackResult<()> {
        let metadata = storage.venv_metadata(self.id);
        let backup_metadata = storage.venv_backup_metadata(self.id);
        let existed = metadata.exists();
        let parent = metadata.parent().with_context_internal(|| {
            format!("path `{}` does not have a parent?", metadata.display())
        })?;
        if !parent.exists() {
            parent.mkdir(MkdirOptions::WithParents)?;
        }
        if existed {
            // move old current state to backup file, as when error occurs during
            // overwriting the main file, the invariants will be upkept.
            // (the backup file will be valid)
            metadata.copy_file_to(&backup_metadata)?;
        }
        self.data.save_to(&metadata)?;
        // also initialize the `.old` file, such that issues
        // relating to unavailable directory `fsync` are minimized
        metadata.copy_file_to(&backup_metadata)?;
        storage.venv_dir(self.id).try_fsync_dir()?;
        Ok(())
    }
}
