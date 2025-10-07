use std::collections::HashMap;

pub struct TyposCfg {
    fixes_enabled: bool,
    max_fix_dist: u32,
}

pub struct DuckCfg {
    aliases: HashMap<String, String>,
    typos_cfg: TyposCfg,
}

impl DuckCfg {
    pub fn default() -> DuckCfg {
        DuckCfg {
            aliases: HashMap::new(),
            typos_cfg: TyposCfg {
                fixes_enabled: false,
                max_fix_dist: 1,
            },
        }
    }

    pub fn from_toml_file() -> DuckCfg {
        // TODO: implement
        !panic!();
    }
}
