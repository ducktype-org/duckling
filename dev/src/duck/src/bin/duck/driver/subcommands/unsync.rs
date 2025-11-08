use anyhow::bail;
use clap::{ArgMatches, Command};
use duck_lib::QuackResult;

use crate::{
    DuckCtx,
    driver::cli_ext::{optional, subcommand},
};

pub fn get_parser() -> Command {
    subcommand("unsync")
        .about("Unsynchronize current or choses venv by removing its state from the storage")
        .arg(optional("venv-id", "Id of the venv to unsynchronize"))
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement unsync")
}
