// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module dealing with all of the posible user configuration of `duck`.
//! The submodules are:
//! - [`duck_home_path`] for determining the location of duck home;
//! - [`configuration_file`] abstracting over the configuration file at duck home.
//!
//! This module notably contains [`DuckCfg`] which is the primary source of information for determining the configuration.
//! It joins the configuration from the file with the configuration via environmental variables.

use std::path::Path;
use std::time::Duration;

use url::Url;

use crate::QuackResult;
use crate::duck::driver::cli_args_preprocessing::aliases_expansion::{Alias, Aliases};
use crate::duck::util::duck_cfg::configuration_file::DuckConfigurationFile;
use crate::duck::util::duck_cfg::duck_home_path::duck_home_path;
use crate::duck::util::duck_home::DuckHome;
use crate::util::env::Env;

pub(super) mod configuration_file;
mod duck_home_path;

/// Environmental variable for overriding the `security.typos.enabled` field of the configuration file.
const FIXES_ENABLED_ENV: &str = "DUCK_SECURITY_TYPOS_ENABLED";

/// Environmental variable for overriding the `security.typos.max-distance` field of the configuration file.
const MAX_FIX_DIST_ENV: &str = "DUCK_SECURITY_TYPOS_MAXDISTANCE";

/// Environmental variable for overriding the `storage.temporary-lifetime` field of the configuration file.
const STORAGE_LIFETIME_ENV: &str = "DUCK_STORAGE_TEMPORARYLIFETIME";

/// Environmental variable for overriding the `registry.url` field of the configuration file.
const REGISTRY_URL_ENV: &str = "DUCK_REGISTRY_URL";

#[derive(Debug)]
/// Struct for determining the user configuration of `duck`.
///
/// Priority of configuration
/// -------------------------
/// For most of the configurable values, the value is determined:
/// 1. By the appropriate environmental variable.
/// 2. If the variable is not set, by the [`DuckConfigurationFile`].
///
/// An exception are aliases, which cannot be set with an environmental variable.
pub struct DuckCfg {
    /// Snapshot of the environmental variables (to look for overrides).
    env: Env,
    /// Parsed configuration file, with sensible defaults.
    cfg_file: DuckConfigurationFile,
}

impl DuckCfg {
    /// Get all known aliases.
    ///
    /// Important:
    /// ----------
    /// The value from [`DuckConfigurationFile`] cannot be overridden by any environmental variable.
    pub fn aliases(&self) -> QuackResult<Aliases> {
        self.cfg_file.aliases()
    }

    /// Get an alias for the given `key`.
    ///
    /// Important:
    /// ----------
    /// The value from [`DuckConfigurationFile`] cannot be overridden by any environmental variable.
    pub fn alias_for(&self, key: &str) -> QuackResult<Option<Alias>> {
        self.cfg_file().alias_for(key)
    }

    /// Whether we should autofix unknown subcommands.
    pub fn fixes_enabled(&self) -> QuackResult<bool> {
        self.env
            .get_bool(FIXES_ENABLED_ENV)?
            .map(Ok)
            .unwrap_or_else(|| self.cfg_file.fixes_enabled())
    }

    /// Maximal distance for autofixing unknown subcommands.
    pub fn max_fix_dist(&self) -> QuackResult<u64> {
        self.env
            .get_u64(MAX_FIX_DIST_ENV)?
            .map(Ok)
            .unwrap_or_else(|| self.cfg_file.max_fix_dist())
    }

    /// Get the lifetime of temporary storage venvs.
    pub fn storage_tmp_lifetime(&self) -> QuackResult<Duration> {
        self.env
            .get_duration(STORAGE_LIFETIME_ENV)?
            .map(Ok)
            .unwrap_or_else(|| self.cfg_file.storage_tmp_lifetime())
    }

    /// Get the default registry url.
    pub fn registry_url(&self) -> QuackResult<Url> {
        self.env
            .get_url(REGISTRY_URL_ENV)?
            .map(Ok)
            .unwrap_or_else(|| self.cfg_file.registry_url())
    }

    /// Get access to the underlying [`DuckConfigurationFile`].
    pub fn cfg_file(&self) -> &DuckConfigurationFile {
        &self.cfg_file
    }

    /// Get mutable access to the underlying [`DuckConfigurationFile`].
    pub fn cfg_file_mut(&mut self) -> &mut DuckConfigurationFile {
        &mut self.cfg_file
    }

    /// Get the maximal allowed amount of opened connections.
    pub fn max_open_connections(&self) -> usize {
        self.cfg_file.max_open_connections()
    }
}

/// Create [`DuckCfg`] and [`DuckHome`].
pub fn get_duck_cfg_and_home(user_home: &Path) -> QuackResult<(DuckCfg, DuckHome)> {
    // This is joined into one function, because `DuckCfg` needs `DuckHome` and `DuckHome` needs `Env`.
    let env = Env::default();
    let duck_home = DuckHome::new(duck_home_path(&env, user_home));
    let cfg_file = DuckConfigurationFile::new(&duck_home)?;
    let cfg = DuckCfg { env, cfg_file };
    Ok((cfg, duck_home))
}
