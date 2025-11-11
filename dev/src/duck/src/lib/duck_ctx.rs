use rustvil::os::env::Env;

use crate::{QuackResult, duck_cfg::DuckCfg, terminal::Terminal, toml_config::TomlConfig};

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
        let console = Terminal::stdout();
        let error_console = Terminal::stderr();
        let config = DuckCfg::new(&env, &error_console)?;
        Ok(Self {
            console,
            error_console,
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

    #[cfg(feature = "test_utils")]
    pub fn duck_cfg_mut(&mut self) -> &mut DuckCfg {
        &mut self.duck_cfg
    }

    pub fn toml_cfg(&self) -> &TomlConfig {
        self.duck_cfg().toml_config()
    }

    pub fn env(&self) -> &Env {
        &self.env
    }

    pub fn env_mut(&mut self) -> &mut Env {
        &mut self.env
    }
}
