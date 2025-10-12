use std::collections::HashMap;

use quackpack::QuackResult;

#[derive(Debug)]
struct TyposCfg {
    fixes_enabled: bool,
    max_fix_dist: u32,
}

#[derive(Debug)]
pub struct DuckCfg {
    aliases: HashMap<String, String>,
    typos_cfg: TyposCfg,
}

impl DuckCfg {
    // TODO: This will likely change, when we'll start to parse real files.
    pub fn new() -> QuackResult<DuckCfg> {
        Ok(DuckCfg {
            aliases: HashMap::new(),
            typos_cfg: TyposCfg {
                fixes_enabled: false,
                max_fix_dist: 1,
            },
        })
    }

    pub fn fixes_enabled(&self) -> bool {
        self.typos_cfg.fixes_enabled
    }

    pub fn max_fix_dist(&self) -> u32 {
        self.typos_cfg.max_fix_dist
    }
}
