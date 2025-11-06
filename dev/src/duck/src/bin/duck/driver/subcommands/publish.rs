use duck_lib::QuackResult;
use anyhow::bail;
use clap::{ArgMatches, Command};

use crate::{DuckCtx, driver::cli_ext::subcommand};

pub fn get_parser() -> Command {
    subcommand("publish").about("Publish package to the registry")
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement publish")
}
