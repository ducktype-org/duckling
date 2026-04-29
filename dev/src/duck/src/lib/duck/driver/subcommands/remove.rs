use crate::{DuckContext, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{CommandExt, flag, subcommand};

/// Creates parser for the `remove` subcommand.
pub fn get_parser() -> Command {
    subcommand("remove")
        .about("Remove the packages from the current venv")
        .arg(flag("global", "Remove the packages from the global venv instead").short('g'))
        .add_dev("Remove dev dependencies")
        .add_packages("Packages to remove")
}

/// Logic for executing the `remove` subcommand.
pub fn execute(_ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement remove")
}
