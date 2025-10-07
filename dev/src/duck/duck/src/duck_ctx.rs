use crate::duck_cfg;
use console::Term;

pub struct DuckCtx {
    console: Term,
    error_console: Term,
    duck_cfg: duck_cfg::DuckCfg, // Wrapper for a deserialized TOML.
}
