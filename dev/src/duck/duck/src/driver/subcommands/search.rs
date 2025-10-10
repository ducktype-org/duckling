use anyhow::bail;
use clap::{Arg, ArgMatches, Command};
use quackpack::QuackResult;

use crate::{DuckCtx, driver::cli_ext::subcommand};

pub fn get_parser() -> Command {
    subcommand("search")
        .about("Search for a package in the registry")
        .arg(Arg::new("package").help("Package name"))
}

pub fn execute(ctx: &DuckCtx, matches: ArgMatches) -> QuackResult<()> {
    bail!("implement search")
}
