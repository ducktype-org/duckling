use std::{
    env::current_dir,
    path::{Path, PathBuf},
};

use anyhow::Context;
use rustvil::{config_files::home, os::env::Env};

use crate::{
    QuackResult,
    duck::util::{duck_cfg::DuckCfg, duck_home::DuckHome, terminal::Terminal},
    quackpack::util::paths::duck_home_path,
};

#[derive(Debug)]
pub struct DuckCtx {
    console: Terminal,
    error_console: Terminal,
    duck_cfg: DuckCfg,
    cwd: PathBuf,
    user_home: PathBuf,
    duck_home: DuckHome,
    env: Env,
}

impl DuckCtx {
    pub fn new() -> QuackResult<Self> {
        let env = Env::default();
        let console = Terminal::stdout();
        let error_console = Terminal::stderr();
        let user_home = home().context("while trying to get user home directory")?;
        let duck_home = DuckHome::new(
            duck_home_path(&env, &user_home).context("while trying to get duck home directory")?,
            &env,
        );
        let config = DuckCfg::new(&duck_home)?;
        let cwd = current_dir().context("while trying to get the current working directory")?;
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

    #[cfg(test)]
    pub fn duck_cfg_mut(&mut self) -> &mut DuckCfg {
        &mut self.duck_cfg
    }

    pub fn env(&self) -> &Env {
        &self.env
    }

    /// Get a path to the current working directory.
    pub fn cwd(&self) -> &Path {
        &self.cwd
    }

    /// Reload the current working directory.
    pub fn reload_cwd(&mut self) -> QuackResult<()> {
        self.cwd = current_dir().context("while trying to get current working directory")?;
        Ok(())
    }

    pub fn user_home(&self) -> &Path {
        &self.user_home
    }

    pub fn duck_home(&self) -> &DuckHome {
        &self.duck_home
    }
}

#[cfg(test)]
impl Default for DuckCtx {
    fn default() -> Self {
        let env = Default::default();
        let user_home = home().unwrap();
        let duck_home = DuckHome::new(duck_home_path(&env, &user_home).unwrap(), &env);
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
