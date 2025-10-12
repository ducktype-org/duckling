use anyhow::bail;
use clap::{Command, value_parser};
use clap_complete::Shell;
use quackpack::QuackResult;

use crate::DuckCtx;
use crate::driver::cli_ext::{CommandExt, optional, subcommand};
use clap::ArgMatches;

pub fn get_parser() -> Command {
    subcommand("generate").about("Generate shell completions")
        .arg(optional("generator", "Choose target shell").value_parser(value_parser!(Shell)))
}

pub fn execute(ctx: &DuckCtx, matches: ArgMatches) -> QuackResult<()> {
    bail!("implement generate")
}
