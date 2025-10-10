use anyhow::bail;
use clap::{ArgMatches, Command};
use quackpack::QuackResult;

use crate::{
    DuckCtx,
    driver::cli_ext::{CommandExt, flag, multi, subcommand},
};

pub fn get_parser() -> Command {
    subcommand("run")
        .about("Build a current package and run it")
        .add_profile()
        .add_release()
        .add_features_conflicting("Enable features of target package to build", "all-features")
        .arg(flag("all-features", "Use all possible features").conflicts_with("features"))
        .add_jobs()
        .arg(multi("args", "Arguments passed to compiled binary"))
}

pub fn execute(ctx: &DuckCtx, matches: ArgMatches) -> QuackResult<()> {
    bail!("implement run")
}
