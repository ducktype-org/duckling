use crate::duck_cfg::{self, DuckCfg};
use crate::terminal::Terminal;

pub struct DuckCtx {
    console: Terminal,
    error_console: Terminal,
    duck_cfg: duck_cfg::DuckCfg,
}

impl DuckCtx {
    pub fn default() -> DuckCtx {
        DuckCtx {
            console: Terminal::stdout(),
            error_console: Terminal::stderr(),
            duck_cfg: DuckCfg::default(),
        }
    }
}
