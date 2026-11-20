// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::env::{current_dir, home_dir};
use std::path::{Path, PathBuf};
use std::str::FromStr;

use super::terminal::Verbosity;
use crate::context::duck_cfg::DuckCfg;
use crate::context::duck_home::DuckHome;
use crate::context::duck_home::duck_home_path;
use crate::context::terminal::Terminal;
use crate::util::env::Env;
use crate::util::file_locks::FileLockManager;
use crate::{QuackResult, QuackResultContext};

#[derive(Debug)]
pub struct DuckContext {
    term: Terminal,
    duck_cfg: DuckCfg,
    cwd: PathBuf,
    user_home: PathBuf,
    duck_home: DuckHome,
    env: Env,
    offline: bool,
}

macro_rules! forward_printing_helpers {
    (
        $to:ident,
        $( $name:ident )+
    ) => {
        $(
            /// Print a
            #[doc = stringify!($name)]
            /// message to the terminal.
            pub fn $name(&self, text: impl ::std::fmt::Display) -> QuackResult<()> {
                self.$to.$name(text)
            }
        )+
    }
}

impl DuckContext {
    /// Create a new [`DuckContext`].
    pub fn new() -> QuackResult<Self> {
        let env = Env::default();
        let term = Terminal::default();
        let user_home = home_dir().context("failed to get the user home directory")?;
        let duck_home = DuckHome::new(duck_home_path(&env, &user_home));
        let config = DuckCfg::new(&duck_home)?;
        let cwd = current_dir().context("failed to get the current working directory")?;
        Ok(Self {
            term,
            duck_cfg: config,
            cwd,
            user_home,
            duck_home,
            env,
            offline: false,
        })
    }

    /// Get the [`Terminal`] for printing.
    pub fn term(&self) -> &Terminal {
        &self.term
    }

    /// Get the mutable [`Terminal`] for printing.
    pub fn term_mut(&mut self) -> &mut Terminal {
        &mut self.term
    }

    /// Get the [`DuckCfg`].
    pub fn duck_cfg(&self) -> &DuckCfg {
        &self.duck_cfg
    }

    #[cfg(any(test, feature = "test-utils"))]
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
        self.cwd = current_dir().context("failed to get the current working directory")?;
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
    pub fn default_storage_root(&self) -> FileLockManager {
        self.duck_home().storage()
    }

    /// Get the maximal allowed amount of opened connections.
    pub fn max_open_connections(&self) -> usize {
        self.duck_cfg.max_open_connections()
    }

    // ------------------
    // Printing functions
    // ------------------
    forward_printing_helpers! {
        term,
        error error_verbose
        warning warning_verbose
        print print_verbose
        info info_verbose
        hint hint_verbose
        note note_verbose
    }

    /// Get a [`String`] input from the user.
    pub fn prompt_once(&self, prompt: impl Into<String>) -> QuackResult<String> {
        self.term.prompt_once(prompt)
    }

    /// Get a [`String`] input from the user as password.
    /// This means that the inputted letters are invisible.
    pub fn password_once(&self, prompt: impl Into<String>) -> QuackResult<String> {
        self.term.password_once(prompt)
    }

    /// Get a [`String`] input from the user, with a default value supplied.
    pub fn prompt_once_with_default(
        &self,
        prompt: impl Into<String>,
        default: String,
    ) -> QuackResult<String> {
        self.term.prompt_once_with_default(prompt, default)
    }

    /// Prompt user for an input until it can be correctly deserialized.
    pub fn prompt_until_valid<T>(&self, prompt: impl Into<String> + Clone) -> T
    where
        T: ToString + FromStr + Clone,
        <T as std::str::FromStr>::Err: std::fmt::Display,
    {
        self.term.prompt_until_valid(prompt)
    }

    /// Prompt user for an input until it can be correctly deserialized, with a default value supplied.
    pub fn prompt_until_valid_with_default<T>(
        &self,
        prompt: impl Into<String> + Clone,
        default: T,
    ) -> T
    where
        T: ToString + FromStr + Clone,
        <T as std::str::FromStr>::Err: std::fmt::Display,
    {
        self.term.prompt_until_valid_with_default(prompt, default)
    }

    /// Get the verbosity of the stderr handler.
    pub fn verbosity(&self) -> Verbosity {
        self.term.verbosity()
    }
}

#[cfg(any(test, feature = "test-utils"))]
impl Default for DuckContext {
    fn default() -> Self {
        use crate::context::terminal::Verbosity;

        let env = Default::default();
        let user_home = home_dir().unwrap();
        let duck_home = DuckHome::new(duck_home_path(&env, &user_home));
        let mut term = Terminal::default();
        term.set_verbosity(Verbosity::Quiet);
        Self {
            term,
            duck_cfg: DuckCfg::new(&duck_home).unwrap(),
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
        assert_send::<DuckContext>();
        assert_sync::<DuckContext>();
    }
}
