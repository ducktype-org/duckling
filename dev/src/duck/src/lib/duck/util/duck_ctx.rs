use std::{
    env::{current_dir, home_dir},
    path::{Path, PathBuf},
};

use crate::{
    QuackResult, QuackResultContext,
    duck::util::{duck_cfg::DuckCfg, duck_home::DuckHome, terminal::Terminal},
    quackpack::util::paths::duck_home_path,
    util::env::Env,
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
    offline: bool,
}

impl DuckCtx {
    /// Create a new [`DuckCtx`].
    pub fn new() -> QuackResult<Self> {
        let env = Env::default();
        let console = Terminal::stdout();
        let error_console = Terminal::stderr();
        let user_home = home_dir().context("while trying to get user home directory")?;
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
            offline: false,
        })
    }

    /// Get the [`Terminal`] for stdout.
    pub fn console(&self) -> &Terminal {
        &self.console
    }

    /// Get the [`Terminal`] for stdout.
    pub fn console_mut(&mut self) -> &mut Terminal {
        &mut self.console
    }

    /// Get the [`Terminal`] for stderr.
    pub fn error_console(&self) -> &Terminal {
        &self.error_console
    }

    /// Get the [`Terminal`] for stderr.
    pub fn error_console_mut(&mut self) -> &mut Terminal {
        &mut self.error_console
    }

    /// Get the [`DuckCfg`].
    pub fn duck_cfg(&self) -> &DuckCfg {
        &self.duck_cfg
    }

    #[cfg(test)]
    pub fn duck_cfg_mut(&mut self) -> &mut DuckCfg {
        &mut self.duck_cfg
    }

    /// Get the snapshot of all environmental variables.
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

    /// Get the path to the user home directory.
    pub fn user_home(&self) -> &Path {
        &self.user_home
    }

    /// Get the [`DuckHome`] layout.
    pub fn duck_home(&self) -> &DuckHome {
        &self.duck_home
    }

    /// Whether we ignore any HTTP requests.
    pub fn is_offline(&self) -> bool {
        self.offline
    }

    /// Set, whether we should ignore any HTTP requests.
    pub fn set_offline(&mut self, offline: bool) {
        self.offline = offline;
    }

    /// Get the path of the default packages' storage in [`DuckHome`].
    pub fn default_storage_root(&self) -> &Path {
        self.duck_home().storage_dir()
    }
}

#[cfg(test)]
impl Default for DuckCtx {
    fn default() -> Self {
        let env = Default::default();
        let user_home = home_dir().unwrap();
        let duck_home = DuckHome::new(duck_home_path(&env, &user_home).unwrap(), &env);
        Self {
            console: Terminal::stdout(),
            error_console: Terminal::stderr(),
            duck_cfg: Default::default(),
            env,
            cwd: current_dir().unwrap(),
            duck_home,
            user_home,
            offline: false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn assert_send_sync_package() {
        fn assert_send<T: Send>() {}
        fn assert_sync<T: Sync>() {}
        assert_send::<DuckCtx>();
        assert_sync::<DuckCtx>();
    }
}
