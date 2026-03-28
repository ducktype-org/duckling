use std::fs;

use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

/// Creates parser for the `deactivate` subcommand.
pub fn get_parser() -> Command {
    subcommand("deactivate").about("Deactivate the active virtual environment")
}

/// Logic for executing the `deactivate` subcommand.
pub fn execute(ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    let active_venv_file = ctx.duck_home().active_venv_file();
    if !active_venv_file.exists() {
        qp_bail!("No venv is active");
    }
    fs::remove_file(active_venv_file)?;
    Ok(())
}
