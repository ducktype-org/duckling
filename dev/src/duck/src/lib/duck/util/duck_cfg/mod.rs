use std::{path::Path, time::Duration};

use url::Url;

use crate::duck::util::duck_cfg::duck_home_path::duck_home_path;
use crate::{
    QuackResult,
    duck::{
        driver::cli_args_preprocessing::aliases_expansion::{Alias, Aliases},
        util::{duck_cfg::configuration_file::DuckConfigurationFile, duck_home::DuckHome},
    },
    util::env::Env,
};

pub(super) mod configuration_file;
mod duck_home_path;

#[derive(Debug)]
pub struct DuckCfg {
    env: Env,
    cfg_file: DuckConfigurationFile,
}

impl DuckCfg {
    /// Get all known aliases.
    pub fn aliases(&self) -> QuackResult<Aliases> {
        self.cfg_file.aliases()
    }

    /// Get an alias for the given `key`.
    pub fn alias_for(&self, key: &str) -> QuackResult<Option<Alias>> {
        self.cfg_file().alias_for(key)
    }

    /// Whether we should autofix unknown subcommands.
    pub fn fixes_enabled(&self) -> QuackResult<bool> {
        self.cfg_file.fixes_enabled()
    }

    /// Maximal distance for autofixing unknown subcommands.
    pub fn max_fix_dist(&self) -> QuackResult<u64> {
        self.cfg_file.max_fix_dist()
    }

    /// Get the lifetime of temporary storage venvs.
    pub fn storage_tmp_lifetime(&self) -> QuackResult<Duration> {
        self.cfg_file.storage_tmp_lifetime()
    }

    /// Get the default registry url.
    pub fn registry_url(&self) -> QuackResult<Url> {
        self.cfg_file.registry_url()
    }

    pub fn cfg_file(&self) -> &DuckConfigurationFile {
        &self.cfg_file
    }

    pub fn cfg_file_mut(&mut self) -> &mut DuckConfigurationFile {
        &mut self.cfg_file
    }

    /// Get the maximal allowed amount of opened connections.
    pub fn max_open_connections(&self) -> usize {
        self.cfg_file.max_open_connections()
    }
}

pub fn get_duck_cfg_and_home(user_home: &Path) -> QuackResult<(DuckCfg, DuckHome)> {
    let env = Env::default();
    let duck_home = DuckHome::new(duck_home_path(&env, user_home));
    let cfg_file = DuckConfigurationFile::new(&duck_home)?;
    let cfg = DuckCfg { env, cfg_file };
    Ok((cfg, duck_home))
}
