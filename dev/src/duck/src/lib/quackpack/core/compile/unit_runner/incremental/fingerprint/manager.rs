// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Manager for locking, reading and writing fingerprints.
//!
//! Layout is:
//! .fingerprint/
//! ├── .lock
//! └── data.json

use std::path::PathBuf;

use crate::util::file_locks::{FileLockManager, LockedFile};
use crate::{DuckContext, QuackResult};

const LOCK_NAME: &str = ".lock";
const FPRINT_NAME: &str = "data.json";

#[derive(Debug)]
/// Manager of the fingerprint directory.
///
/// Inner [`FileLockManager`] is the `.fingerprint/` directory.
///
/// This struct doesn't expose any methods for reading or writing the actual data.
/// You need to call [`FingerprintManager::lock`] and use [`ExclusiveFingerprintGuard`].
pub struct FingerprintManager {
    manager: FileLockManager,
}

impl FingerprintManager {
    /// Create a new [`FingerprintManager`].
    pub fn new(manager: FileLockManager) -> Self {
        Self { manager }
    }

    /// Lock exclusively this [`FingerprintManager`].
    pub fn lock(&mut self, ctx: &DuckContext) -> QuackResult<ExclusiveFingerprintGuard<'_>> {
        let lock = self.manager.open_exclusive(LOCK_NAME, ctx)?;
        Ok(ExclusiveFingerprintGuard {
            manager: self,
            _lock: lock,
        })
    }
}

#[derive(Debug)]
/// A locked version of [`FingerprintManager`].
///
/// Only this struct allows reading and writing fingerprint data.
pub struct ExclusiveFingerprintGuard<'a> {
    // Mutable to make sure that nobody else can use original [`FingerprintManager`] while this
    // struct is alive.
    manager: &'a mut FingerprintManager,
    _lock: LockedFile,
}

impl ExclusiveFingerprintGuard<'_> {
    /// Read [`BuildData`] from the fingerprint file.
    ///
    /// [`BuildData`]: super::BuildData
    pub fn read_build_data(&self) -> QuackResult<super::BuildData> {
        let path = self.fingerprint_path();
        super::BuildData::from_fingerprint(&path)
    }

    /// Write [`BuildData`] to the fingerprint file.
    ///
    /// [`BuildData`]: super::BuildData
    pub fn write_build_data(&self, data: super::BuildData) -> QuackResult<()> {
        let path = self.fingerprint_path();
        data.write_fingerprint(&path)
    }

    /// Get the path to the fingerprint file with data.
    pub fn fingerprint_path(&self) -> PathBuf {
        self.manager.manager.not_locked_path().join(FPRINT_NAME)
    }
}
