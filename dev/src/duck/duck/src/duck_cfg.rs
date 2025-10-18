use anyhow::Context;
use quackpack::{QuackResult, paths::config_file, toml_config::TomlConfig};
use rustvil::os::env::Env;

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
}
