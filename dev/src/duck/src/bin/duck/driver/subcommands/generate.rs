use duck_lib::QuackResult;
use anyhow::bail;
use clap::{Command, value_parser};
use clap_complete::Shell;

use crate::DuckCtx;
use crate::driver::cli_ext::{optional, subcommand};
use clap::ArgMatches;

pub fn get_parser() -> Command {
    subcommand("generate")
        .about("Generate shell completions")
        .arg(optional("generator", "Choose target shell").value_parser(value_parser!(Shell)))
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement generate")
}
