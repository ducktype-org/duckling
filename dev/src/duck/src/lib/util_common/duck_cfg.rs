use anyhow::Context;
use rustvil::os::env::Env;
use tracing::debug;

use crate::{
    QuackResult,
    quackpack::util::paths::config_file,
    util_common::{terminal::Terminal, toml_config::TomlConfig},
};

#[derive(Debug, Default)]
pub struct DuckCfg {
    inner: TomlConfig,
}

impl DuckCfg {
    pub fn new(env: &Env, term: &Terminal) -> QuackResult<DuckCfg> {
        let inner = match config_file(env) {
            Some(path) => {
                debug!("reading user config from `{}`", path.display());
                TomlConfig::new(path)?
            }
            None => {
                term.warning("couldn't detect user config path, falling back to defaults...");
                TomlConfig::default()
            }
        };
        debug!("parsed user config `{inner:?}`");
        Ok(Self { inner })
    }

    pub fn fixes_enabled(&self) -> QuackResult<bool> {
        Ok(self
            .inner
            .get_bool("security.typos.enabled")
            .context("when trying to determine whether typos fixing is enabled")?
            .unwrap_or(false))
    }

    pub fn max_fix_dist(&self) -> QuackResult<u32> {
        self.inner
            .get_int("security.typos.max_distance")
            .context("when trying to check maximum typos fixing distance")?
            .unwrap_or(3)
            .try_into()
            .with_context(|| self.inner.make_location_error())
            .context("maximum typos fixing distance does not fit in `u32`")
            .context("when getting key `security.typos.max_distance`")
            .context("when trying to check maximum typos fixing distance")
    }

    pub fn aliases(&self) -> QuackResult<Option<impl Iterator<Item = &String>>> {
        let Some(aliases) = self
            .inner
            .get_table("aliases")
            .context("when trying to get all user-defined aliases")?
        else {
            return Ok(None);
        };
        Ok(Some(aliases.keys()))
    }

    pub fn alias_for(&self, key: &str) -> QuackResult<Option<&str>> {
        self.inner
            .get_str(&format!("aliases.{key}"))
            .with_context(|| format!("when trying to get alias expansions `{key}`"))
    }

    pub fn toml_config(&self) -> &TomlConfig {
        &self.inner
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
