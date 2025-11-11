use anyhow::bail;
use clap::{ArgMatches, Command};
use duck::{DuckCtx, QuackResult};

use crate::driver::cli_ext::{CommandExt, flag, subcommand};

pub fn get_parser() -> Command {
    subcommand("build")
        .about("Build a current package")
        .add_profile()
        .add_release()
        .add_features_conflicting("Enable features of target package to build", "all-features")
        .arg(flag("all-features", "Use all possible features").conflicts_with("features"))
        .add_jobs()
}
pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement build")
}
