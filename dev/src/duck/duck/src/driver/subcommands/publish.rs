use anyhow::bail;
use clap::{ArgMatches, Command};
use quackpack::QuackResult;

use crate::{DuckCtx, driver::cli_ext::subcommand};

pub fn get_parser() -> Command {
    subcommand("publish").about("Publish package to the registry")
}

pub fn execute(ctx: &DuckCtx, matches: ArgMatches) -> QuackResult<()> {
    bail!("implement publish")
}
