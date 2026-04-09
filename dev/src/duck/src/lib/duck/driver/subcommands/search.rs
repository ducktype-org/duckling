use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{Arg, ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

/// Creates parser for the `search` subcommand.
pub fn get_parser() -> Command {
    subcommand("search")
        .about("Search for a package in the registry")
        .arg(Arg::new("package").help("Package name"))
}

/// Logic for executing the `search` subcommand.
pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement search")
}
