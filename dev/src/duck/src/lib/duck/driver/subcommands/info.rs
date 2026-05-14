use crate::{DuckContext, QuackResult, qp_bail};
use clap::{Arg, ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

/// Creates parser for the `info` subcommand.
pub fn get_parser() -> Command {
    subcommand("info")
        .about("Get package information")
        .arg(Arg::new("package").help("Package name"))
        .arg(Arg::new("version").help("Package version"))
}

/// Logic for executing the `info` subcommand.
pub fn execute(_ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement info")
}
