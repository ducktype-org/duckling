use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use crate::{QuackResult, util_common::toml_config::TomlConfig};

#[derive(Debug, Default, Serialize, Deserialize)]
#[serde(transparent)]
pub struct VenvConfig {
    config: TomlConfig,
}

impl VenvConfig {
    pub fn new(path: PathBuf) -> QuackResult<Self> {
        Ok(Self {
            config: TomlConfig::new(path)?,
        })
    }

    pub fn is_ephermal(&self) -> QuackResult<bool> {
        Ok(self.config.get_bool("ephermal")?.unwrap_or(false))
    }

    pub fn storage_path(&self) -> QuackResult<Option<&Path>> {
        self.config.get_path("local_storage")
    }

    pub fn is_freezefile_exposed(&self) -> QuackResult<bool> {
        Ok(self.config.get_bool("expose_freezefile")?.unwrap_or(false))
    }

    pub fn set_ephermal(&mut self, value: bool) -> QuackResult<()> {
        self.config.set_bool("ephermal", value)
    }

    pub fn set_storage_path(&mut self, path: &Path) -> QuackResult<()> {
        self.config.set_path("local_storage", path)
    }

    pub fn set_freezefile_exposed(&mut self, value: bool) -> QuackResult<()> {
        self.config.set_bool("expose_freezefile", value)
    }
}
