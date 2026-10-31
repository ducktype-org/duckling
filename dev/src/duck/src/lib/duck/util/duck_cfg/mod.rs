// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Abstraction over the duck configuration file, located at `<duck's home root>/config.yaml`.

use std::sync::OnceLock;
use std::time::Duration;

use tracing::trace;
use url::Url;

use crate::duck::driver::cli_args_preprocessing::aliases_expansion::{Alias, Aliases};
use crate::duck::util::duck_home::DuckHome;
use crate::quackpack::core::fetcher;
use crate::quackpack::schemas::config::{RegistryConfig, SecurityConfig, StorageConfig};
use crate::util::env::Env;
use crate::util::once_lock_ext::OnceLockExt;
use crate::util::yaml_config::YamlConfig;
use crate::{QuackResult, QuackResultContext};

pub(super) mod duck_home_path;

#[derive(Debug, Default)]
/// Abstraction over the duck configuration file, located at `<duck's home root>/config.yaml`.
/// Provides the sensible defaults for fields not present in the file.
pub struct DuckCfg {
    inner: YamlConfig,
    // Fast config accessors.
    security: OnceLock<SecurityConfig>,
    registry: OnceLock<RegistryConfig>,
    storage: OnceLock<StorageConfig>,
}

// @TODO: #3318 Use `from_hours(24)`, after bumping rust's version in CI to 1.91.0.
/// The default lifetime of a temporary venv in a storage.
const DEFAULT_STORAGE_LIFETIME: Duration = Duration::from_secs(24 * 60 * 60);

/// The default radius in which we'll attempt to autofix a subcommand.
const DEFAULT_MAXIMAL_AUTOFIX_DISTANCE: u64 = 3;

/// A random, but reasonable(?) value.
const DEFAULT_NUMBER_OF_OPEN_CONNECTIONS: usize = 64;

impl DuckCfg {
    /// Create a new [`DuckCfg`] using config file from the given [`DuckHome`].
    pub fn new(home: &DuckHome) -> QuackResult<DuckCfg> {
        let inner = YamlConfig::new(home.user_config().to_path_buf())?;
        trace!(?inner, "parsed the user config");
        Ok(Self {
            inner,
            security: OnceLock::new(),
            registry: OnceLock::new(),
            storage: OnceLock::new(),
        })
    }

    /// Get the `security:` configuration table.
    pub fn security(&self, env: &Env) -> QuackResult<&SecurityConfig> {
        self.security.try_init_with(|| {
            self.inner
                .deserialize_optional_or_default_with_env_overrides(env, "security")
        })
    }

    /// Get the `registry:` configuration table.
    pub fn registry(&self, env: &Env) -> QuackResult<&RegistryConfig> {
        self.registry.try_init_with(|| {
            self.inner
                .deserialize_optional_or_default_with_env_overrides(env, "registry")
        })
    }

    /// Get the `storage:` configuration table.
    pub fn storage(&self, env: &Env) -> QuackResult<&StorageConfig> {
        self.storage.try_init_with(|| {
            self.inner
                .deserialize_optional_or_default_with_env_overrides(env, "storage")
        })
    }

    /// Whether we should autofix unknown subcommands.
    pub fn fixes_enabled(&self, env: &Env) -> QuackResult<bool> {
        Ok(self
            .security(env)?
            .typos
            .as_ref()
            .and_then(|typos| typos.enabled)
            .unwrap_or(false))
    }

    /// Maximal distance for autofixing unknown subcommands.
    pub fn max_fix_dist(&self, env: &Env) -> QuackResult<u64> {
        Ok(self
            .security(env)?
            .typos
            .as_ref()
            .and_then(|typos| typos.max_distance)
            .unwrap_or(DEFAULT_MAXIMAL_AUTOFIX_DISTANCE))
    }

    /// Get all known aliases.
    pub fn aliases(&self) -> QuackResult<Aliases> {
        Ok(self
            .inner
            .deserialize_optional("aliases")?
            .unwrap_or_default())
    }

    /// Get an alias for the given `key`.
    pub fn alias_for(&self, key: &str) -> QuackResult<Option<Alias>> {
        self.inner
            .deserialize_optional(&format!("aliases.{key}"))
            .with_context(|| format!("when trying to get the alias expansions of `{key}`"))
    }

    /// Get the underlying [`YamlConfig`].
    pub fn yaml_config(&self) -> &YamlConfig {
        &self.inner
    }

    /// Get the lifetime of temporary storage venvs.
    pub fn storage_tmp_lifetime(&self, env: &Env) -> QuackResult<Duration> {
        Ok(self
            .storage(env)?
            .temporary_lifetime
            .map(|duration| duration.0)
            .unwrap_or(DEFAULT_STORAGE_LIFETIME))
    }

    /// Get the default registry url.
    pub fn registry_url(&self, env: &Env) -> QuackResult<Url> {
        let url = self.registry(env)?.url.clone();
        let url = url.unwrap_or_else(|| fetcher::DEFAULT_REGISTRY_URL.clone());
        Ok(url)
    }

    /// Get the maximal allowed amount of opened connections.
    pub fn max_open_connections(&self) -> usize {
        // NOTE: Leaving this as a helper, so that in a future user can configure it.
        DEFAULT_NUMBER_OF_OPEN_CONNECTIONS
    }
}

#[cfg(test)]
mod test_utils {
    use std::collections::HashMap;

    use super::DuckCfg;

    impl DuckCfg {
        pub fn set_aliases(&mut self, new_val: HashMap<String, String>) {
            let val: serde_yaml_ng::Mapping = new_val
                .into_iter()
                .map(|(k, v)| (k.into(), v.into()))
                .collect();
            self.inner.set_table("aliases", val).expect("test");
        }
    }
}
