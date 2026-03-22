use std::time::Duration;

use tracing::debug;

use crate::{
    QuackResult, QuackResultContext, duck::util::duck_home::DuckHome, qp_err,
    util_common::toml_config::TomlConfig,
};

#[derive(Debug, Default)]
pub struct DuckCfg {
    inner: TomlConfig,
}

// !TODO: Use `from_hours(24)`, after bumping rust's version in CI to 1.91.0.
const DEFAULT_STORAGE_LIFETIME: Duration = Duration::from_secs(24 * 60 * 60);

impl DuckCfg {
    /// Create a new [`DuckCfg`] using config file from the given [`DuckHome`].
    pub fn new(home: &DuckHome) -> QuackResult<DuckCfg> {
        let inner = TomlConfig::new(home.user_config().to_path_buf())?;
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
    pub fn max_fix_dist(&self) -> QuackResult<u32> {
        self.inner
            .get_int("security.typos.max_distance")
            .context("when trying to check the maximum typos fixing distance")?
            .unwrap_or(3)
            .try_into()
            .with_context(|| self.inner.make_location_error())
            .context("maximum typos fixing distance does not fit in `u32`")
            .context("when getting the key `security.typos.max_distance`")
            .context("when trying to check the maximum typos fixing distance")
    }

    /// Get all known aliases (keys).
    pub fn aliases(&self) -> QuackResult<Option<impl Iterator<Item = &String>>> {
        let Some(aliases) = self
            .inner
            .get_table("aliases")
            .context("when trying to get all the user-defined aliases")?
        else {
            return Ok(None);
        };
        Ok(Some(aliases.keys()))
    }

    /// Get an alias for the given `key`.
    pub fn alias_for(&self, key: &str) -> QuackResult<Option<&str>> {
        self.inner
            .get_str(&format!("aliases.{key}"))
            .with_context(|| format!("when trying to get the alias expansions of `{key}`"))
    }

    /// Get the underlying [`TomlConfig`].
    pub fn toml_config(&self) -> &TomlConfig {
        &self.inner
    }

    /// Get the lifetime of temporary storage venvs.
    pub fn storage_tmp_lifetime(&self) -> QuackResult<Duration> {
        let config_seconds = self
            .inner
            .get_int("storage.temporary_lifetime")
            .context("when trying to get the storage temporary lifetime")?;
        let Some(secs) = config_seconds else {
            return Ok(DEFAULT_STORAGE_LIFETIME);
        };
        let Ok(as_u64) = secs.try_into() else {
            return Err(qp_err!("{}", self.inner.make_location_error()))
                .context("`storage.temporary_lifetime` does not fit in `u64`");
        };
        Ok(Duration::from_secs(as_u64))
    }
}

#[cfg(test)]
mod test_utils {
    use std::collections::HashMap;

    use toml::{Table, Value};

    use super::DuckCfg;

    impl DuckCfg {
        pub fn set_max_fix_dist(&mut self, new_val: i64) {
            self.inner
                .set_int("security.typos.max_distance", new_val)
                .expect("test");
        }

        pub fn set_fixes_enabled(&mut self, new_val: bool) {
            self.inner
                .set_bool("security.typos.enabled", new_val)
                .expect("test");
        }

        pub fn set_aliases(&mut self, new_val: HashMap<String, String>) {
            let val: Table = new_val
                .into_iter()
                .map(|(k, v)| (k, Value::String(v)))
                .collect();
            self.inner.set_table("aliases", val).expect("test");
        }
    }
}
