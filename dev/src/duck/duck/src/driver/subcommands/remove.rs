use anyhow::bail;
use clap::{ArgMatches, Command};
use quackpack::QuackResult;

use crate::{
    DuckCtx,
    driver::cli_ext::{CommandExt, flag, subcommand},
};

pub fn get_parser() -> Command {
    subcommand("remove")
        .about("Remove packages from the current venv")
        .arg(flag("global", "Remove packages from the global venv instead").short('g'))
        .add_dev("Remove dev dependencies")
        .add_packages("Packages to remove")
}

pub fn execute(ctx: &DuckCtx, matches: ArgMatches) -> QuackResult<()> {
    bail!("implement remove")
}
