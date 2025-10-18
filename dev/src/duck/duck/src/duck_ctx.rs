use std::collections::HashMap;

use crate::{duck_cfg::DuckCfg, terminal::Terminal};
use quackpack::QuackResult;
use rustvil::os::env::Env;

#[derive(Debug)]
pub struct DuckCtx {
    console: Terminal,
    error_console: Terminal,
    duck_cfg: DuckCfg,
    env: Env,
}

impl DuckCtx {
    pub fn new() -> QuackResult<Self> {
        let env = Env::default();
        let config = DuckCfg::new(&env)?;
        Ok(Self {
            console: Terminal::stdout(),
            error_console: Terminal::stderr(),
            duck_cfg: config,
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

    pub fn env(&self) -> &Env {
        &self.env
    }

    pub fn env_mut(&mut self) -> &mut Env {
        &mut self.env
    }

    pub fn alias_for(&self, _name: &str) -> QuackResult<Option<String>> {
        Ok(None)
    }

    pub fn aliases(&self) -> QuackResult<HashMap<String, String>> {
        Ok(HashMap::new())
    }

    pub fn typos_fixes_enabled(&self) -> QuackResult<bool> {
        self.duck_cfg.fixes_enabled()
    }

    pub fn max_fix_dist(&self) -> QuackResult<u32> {
        self.duck_cfg.max_fix_dist()
    }
}
