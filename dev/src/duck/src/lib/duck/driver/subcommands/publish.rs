use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

pub fn get_parser() -> Command {
    subcommand("publish").about("Publish the current package to the registry")
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement publish")
}
