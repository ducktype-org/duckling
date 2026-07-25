use std::time::Duration;

use tracing::debug;

use crate::duck::driver::cli_args_preprocessing::aliases_expansion::{Alias, Aliases};
use crate::duck::util::duck_home::DuckHome;
use crate::util::yaml_config::YamlConfig;
use crate::{QuackResult, QuackResultContext};

#[derive(Debug, Default)]
pub struct DuckCfg {
    inner: YamlConfig,
}

// !TODO: Use `from_hours(24)`, after bumping rust's version in CI to 1.91.0.
/// The default lifetime of a temporary venv in a storage.
const DEFAULT_STORAGE_LIFETIME: Duration = Duration::from_secs(24 * 60 * 60);

/// The default radius in which we'll attempt to autofix a subcommand.
const DEFAULT_MAXIMAL_AUTOFIX_DISTANCE: u64 = 3;

impl DuckCfg {
    /// Create a new [`DuckCfg`] using config file from the given [`DuckHome`].
    pub fn new(home: &DuckHome) -> QuackResult<DuckCfg> {
        let inner = YamlConfig::new(home.user_config().to_path_buf())?;
        debug!("parsed the user config `{inner:?}`");
        Ok(Self { inner })
    }

    /// Whether we should autofix unknown subcommands.
    pub fn fixes_enabled(&self) -> QuackResult<bool> {
        Ok(self
            .inner
            .get_bool("security.typos.enabled")
            .context("when trying to determine whether typos fixing is enabled")?
            .unwrap_or(false))
    }

    /// Maximal distance for autofixing unknown subcommands.
    pub fn max_fix_dist(&self) -> QuackResult<u64> {
        Ok(self
            .inner
            .get_u64("security.typos.max-distance")
            .context("when trying to check the maximum typos fixing distance")?
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
    pub fn storage_tmp_lifetime(&self) -> QuackResult<Duration> {
        let config_seconds = self
            .inner
            .get_u64("storage.temporary-lifetime")
            .context("when trying to get the storage temporary lifetime")?;
        let Some(secs) = config_seconds else {
            return Ok(DEFAULT_STORAGE_LIFETIME);
        };
        Ok(Duration::from_secs(secs))
    }
}

#[cfg(test)]
mod test_utils {
    use std::collections::HashMap;

    use super::DuckCfg;

    impl DuckCfg {
        pub fn set_max_fix_dist(&mut self, new_val: i64) {
            self.inner
                .set_i64("security.typos.max-distance", new_val)
                .expect("test");
        }

        pub fn set_fixes_enabled(&mut self, new_val: bool) {
            self.inner
                .set_bool("security.typos.enabled", new_val)
                .expect("test");
        }

        pub fn set_aliases(&mut self, new_val: HashMap<String, String>) {
            let val: serde_yaml_ng::Mapping = new_val
                .into_iter()
                .map(|(k, v)| (k.into(), v.into()))
                .collect();
            self.inner.set_table("aliases", val).expect("test");
        }
    }
}
