use console::Term;

pub mod duck_cfg;

pub struct DuckCtx {
    console: Term,
    error_console: Term,
    duck_cfg: duck_cfg::DuckCfg; // Wrapper for a deserialized TOML. 
}