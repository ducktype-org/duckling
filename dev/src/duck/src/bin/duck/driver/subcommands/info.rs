use anyhow::bail;
use clap::{Arg, ArgMatches, Command};
use duck::{DuckCtx, QuackResult};

use crate::driver::cli_ext::subcommand;

pub fn get_parser() -> Command {
    subcommand("info")
        .about("Get a package information")
        .arg(Arg::new("package").help("Package name"))
        .arg(Arg::new("version").help("Package version"))
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement info")
}
