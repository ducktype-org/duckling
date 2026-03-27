//! Local venv's configuration.
use std::path::{Path, PathBuf};

use crate::{QuackResult, util_common::yaml_config::YamlConfig};

#[derive(Debug, Default)]
/// Configuration of a package's venv.
pub struct VenvConfig {
    config: YamlConfig,
}

impl VenvConfig {
    /// Create a new [`VenvConfig`] from a config at the given path.
    pub fn new(path: PathBuf) -> QuackResult<Self> {
        Ok(Self {
            config: YamlConfig::new(path)?,
        })
    }

    /// Is this venv ephemeral (temporary).
    pub fn is_ephemeral(&self) -> QuackResult<bool> {
        Ok(self.config.get_bool("ephemeral")?.unwrap_or(false))
    }

    /// Get path to the storage specified in config.
    pub fn storage_path(&self) -> QuackResult<Option<&Path>> {
        self.config.get_path("local_storage")
    }

    /// Should freezefile be exposed to the user.
    pub fn is_freezefile_exposed(&self) -> QuackResult<bool> {
        Ok(self.config.get_bool("expose_freezefile")?.unwrap_or(false))
    }

    /// Set ephemerality. Mostly used by `duck init`.
    pub fn set_ephemeral(&mut self, value: bool) -> QuackResult<()> {
        self.config.set_bool("ephemeral", value)
    }

    /// Set the storage path of this venv. Mostly used by `duck init`.
    pub fn set_storage_path(&mut self, path: &Path) -> QuackResult<()> {
        self.config.set_path("local_storage", path)
    }

    /// Set whether freezefile should be exposed. Mostly used by `duck init`.
    pub fn set_freezefile_exposed(&mut self, value: bool) -> QuackResult<()> {
        self.config.set_bool("expose_freezefile", value)
    }
}
