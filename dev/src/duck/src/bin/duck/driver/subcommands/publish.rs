use anyhow::bail;
use clap::{ArgMatches, Command};
use duck::{DuckCtx, QuackResult};

use crate::driver::cli_ext::subcommand;

pub fn get_parser() -> Command {
    subcommand("publish").about("Publish package to the registry")
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement publish")
}
