use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{optional, subcommand};

/// Creates parser for the `unsync` subcommand.
pub fn get_parser() -> Command {
    subcommand("unsync")
        .about("Unsynchronize the chosen venv by removing its state from the storage")
        .arg(optional("venv-id", "Id of the venv to unsynchronize"))
}

/// Logic for executing the `unsync` subcommand.
pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement unsync")
}
