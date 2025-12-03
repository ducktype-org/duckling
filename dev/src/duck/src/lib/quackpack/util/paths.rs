use rustvil::{fs::PathExt, os::env::Env};
use std::path::{Path, PathBuf};

use crate::QuackResult;

pub const MANIFEST_FILENAME: &str = "quackconfig.yml";
pub const FREEZEFILE_NAME: &str = "quackfreeze.json";
pub const VENV_CONFIG_FILENAME: &str = "venvconfig.toml";
pub const LOCAL_STORAGE_DIR_NAME: &str = ".storage";
pub const DUCK_HOME: &str = "DUCK_HOME";

pub fn duck_home_path(env: &Env, user_home: &Path) -> QuackResult<PathBuf> {
    env.get_os(DUCK_HOME)
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            let mut home = user_home.to_path_buf();
            if cfg!(windows) {
                home.push("AppData");
                home.push("Local");
            } else {
                home.push(".local");
                home.push("share");
            }
            home.push("duck");
            home
        })
        .expand_user()?
        .resolve()
        .map_err(Into::into)
}
