use anyhow::bail;
use clap::{ArgMatches, Command};
use quackpack::QuackResult;

use crate::{
    DuckCtx,
    driver::cli_ext::{CommandExt, flag, subcommand},
};

pub fn get_parser() -> Command {
    subcommand("build")
        .alias("b")
        .about("Build a current package")
        .add_profile()
        .add_release()
        .add_features_conflicting("Enable features of target package to build", "all-features")
        .arg(flag("all-features", "Use all possible features").conflicts_with("features"))
        .add_jobs()
}
pub fn execute(ctx: &DuckCtx, matches: ArgMatches) -> QuackResult<()> {
    bail!("implement build")
}
