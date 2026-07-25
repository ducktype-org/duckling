//! Specific venv configuration.

use std::path::{Path, PathBuf};

use crate::DuckContext;

#[derive(Clone, Debug)]
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

    /// Create a default configuration for the package.
    pub fn default_for_package(ctx: &DuckContext) -> Self {
        let storage = ctx.default_storage_root().into_not_locked_path();
        Self::new(
            storage, /* expose_freezefile */ true, /* ephemeral */ false,
        )
    }

    /// Create a default configuration for the script.
    pub fn default_for_script(ctx: &DuckContext) -> Self {
        let storage = ctx.default_storage_root().into_not_locked_path();
        Self::new(
            storage, /* expose_freezefile */ false, /* ephemeral */ true,
        )
    }
}
