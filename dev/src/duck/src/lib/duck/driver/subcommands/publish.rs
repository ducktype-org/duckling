use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

/// Creates parser for the `publish` subcommand.
pub fn get_parser() -> Command {
    subcommand("publish").about("Publish the current package to the registry")
}

/// Logic for executing the `publish` subcommand.
pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement publish")
}
