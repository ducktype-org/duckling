// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Specific venv configuration.

use std::path::{Path, PathBuf};

use crate::DuckContext;

#[derive(Clone, Debug)]
/// A custom (or default :p) venv configuration.
/// Defaults are inserted if the user doesn't provide custom values, and they can be inspected in
/// `default_for_*` methods.
pub struct VenvConfig {
    storage_path: PathBuf,
    expose_freezefile: bool,
    ephemeral: bool,
}

impl VenvConfig {
    /// Create a new [`VenvConfig`].
    pub fn new(storage_path: PathBuf, expose_freezefile: bool, ephemeral: bool) -> Self {
        Self {
            storage_path,
            expose_freezefile,
            ephemeral,
        }
    }

    /// Get the path to the storage.
    pub fn storage_path(&self) -> &Path {
        &self.storage_path
    }

    /// Should freezefile be exposed.
    pub fn expose_freezefile(&self) -> bool {
        self.expose_freezefile
    }

    /// Is this venv ephemeral (temporary).
    pub fn ephemeral(&self) -> bool {
        self.ephemeral
    }

    /// Create a default configuration for a package.
    pub fn default_for_package(ctx: &DuckContext) -> Self {
        let storage = ctx.default_storage_root().into_not_locked_path();
        Self::new(
            storage, /* expose_freezefile */ true, /* ephemeral */ false,
        )
    }

    /// Create a default configuration for a script.
    pub fn default_for_script(ctx: &DuckContext) -> Self {
        let storage = ctx.default_storage_root().into_not_locked_path();
        Self::new(
            storage, /* expose_freezefile */ false, /* ephemeral */ true,
        )
    }
}
