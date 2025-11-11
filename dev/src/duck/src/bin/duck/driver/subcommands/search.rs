use anyhow::bail;
use clap::{Arg, ArgMatches, Command};
use duck::{DuckCtx, QuackResult};

use crate::driver::cli_ext::subcommand;

pub fn get_parser() -> Command {
    subcommand("search")
        .about("Search for a package in the registry")
        .arg(Arg::new("package").help("Package name"))
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement search")
}
