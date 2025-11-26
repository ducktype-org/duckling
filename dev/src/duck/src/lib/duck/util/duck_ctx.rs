use std::{
    env::current_dir,
    path::{Path, PathBuf},
};

use anyhow::Context;
use rustvil::{config_files::home, os::env::Env};

use crate::{
    QuackResult,
    duck::util::{duck_cfg::DuckCfg, terminal::Terminal},
    quackpack::util::paths::duck_home,
    util_common::toml_config::TomlConfig,
};

#[derive(Debug)]
pub struct DuckCtx {
    console: Terminal,
    error_console: Terminal,
    duck_cfg: DuckCfg,
    cwd: PathBuf,
    user_home: PathBuf,
    duck_home: PathBuf,
    env: Env,
}

impl DuckCtx {
    pub fn new() -> QuackResult<Self> {
        let env = Env::default();
        let console = Terminal::stdout();
        let error_console = Terminal::stderr();
        let config = DuckCfg::new(&env, &error_console)?;
        let cwd = current_dir().context("while trying to get current working directory")?;
        let user_home = home().context("while trying to get user home directory")?;
        let duck_home = duck_home(&env, &user_home);
        Ok(Self {
            console,
            error_console,
            duck_cfg: config,
            cwd,
            user_home,
            duck_home,
            env,
        })
    }

    pub fn console(&self) -> &Terminal {
        &self.console
    }

    pub fn console_mut(&mut self) -> &mut Terminal {
        &mut self.console
    }

    pub fn error_console(&self) -> &Terminal {
        &self.error_console
    }

    pub fn error_console_mut(&mut self) -> &mut Terminal {
        &mut self.error_console
    }

    pub fn duck_cfg(&self) -> &DuckCfg {
        &self.duck_cfg
    }

    pub fn toml_cfg(&self) -> &TomlConfig {
        self.duck_cfg().toml_config()
    }

    #[cfg(test)]
    pub fn duck_cfg_mut(&mut self) -> &mut DuckCfg {
        &mut self.duck_cfg
    }

    pub fn env(&self) -> &Env {
        &self.env
    }

    pub fn env_mut(&mut self) -> &mut Env {
        &mut self.env
    }

    /// Get a path to the current working directory.
    pub fn cwd(&self) -> &Path {
        &self.cwd
    }

    /// Reload a current working directory.
    pub fn reload_cwd(&mut self) -> QuackResult<()> {
        self.cwd = current_dir().context("while trying to get current working directory")?;
        Ok(())
    }

    pub fn user_home(&self) -> &Path {
        &self.user_home
    }

    pub fn duck_home(&self) -> &Path {
        &self.duck_home
    }
}

#[cfg(test)]
impl Default for DuckCtx {
    fn default() -> Self {
        let env = Default::default();
        let user_home = home().unwrap();
        let duck_home = duck_home(&env, &user_home);
        Self {
            console: Terminal::stdout(),
            error_console: Terminal::stderr(),
            duck_cfg: Default::default(),
            env,
            cwd: current_dir().unwrap(),
            duck_home,
            user_home,
        }
    }
}
