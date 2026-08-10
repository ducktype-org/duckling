//! Various path helpers.
use std::path::{Path, PathBuf};

use crate::util::env::Env;
use crate::util::path_ops_ext::PathOpsExt;

/// Environmental variable overriding duck home root.
const DUCK_HOME_ENV: &str = "DUCK_HOME";

/// Get the root to the duck home, given an env snapshot and a user home directory.
pub fn duck_home_path(env: &Env, user_home: &Path) -> PathBuf {
    env.get_os(DUCK_HOME_ENV)
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
        .expand_tilde_with(user_home)
        .resolve()
}
