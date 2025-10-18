use std::collections::HashMap;

use anyhow::Context;
use quackpack::{QuackResult, paths::config_file, toml_config::TomlConfig};
use rustvil::os::env::Env;
use toml::map::Map;

#[derive(Debug, Default)]
pub struct DuckCfg {
    inner: TomlConfig,
}

impl DuckCfg {
    pub fn new(env: &Env) -> QuackResult<DuckCfg> {
        let Some(file) = config_file(env) else {
            return Ok(DuckCfg::default());
        };
        Ok(Self {
            inner: TomlConfig::new(file)?,
        })
    }

    pub fn fixes_enabled(&self) -> QuackResult<bool> {
        Ok(self
            .inner
            .get_bool("security.typos.enabled")?
            .unwrap_or(false))
    }

    pub fn max_fix_dist(&self) -> QuackResult<u32> {
        self.inner
            .get_int("security.typos.max_distance")?
            .unwrap_or(3)
            .try_into()
            .with_context(|| self.inner.make_location_error())
    }

    pub fn aliases(&self) -> QuackResult<HashMap<String, String>> {
        let default_map = Map::new();
        self.inner
            .get_table("aliases")?
            .unwrap_or(&default_map)
            .iter()
            .map(|(k, v)| {
                Ok((
                    k.clone(),
                    String::from(
                        v.as_str()
                            .with_context(|| self.inner.make_location_error())?,
                    ),
                ))
            })
            .collect()
    }

    pub fn alias_for(&self, name: &str) -> QuackResult<Option<String>> {
        Ok(self
            .inner
            .get_str(format!("aliases.{}", name).as_str())?
            .map(String::from))
    }
}
