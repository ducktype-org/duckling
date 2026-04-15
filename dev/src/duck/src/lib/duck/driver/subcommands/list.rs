use crate::{DuckContext, QuackResult, qp_bail};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, optional, subcommand};

/// Creates parser for the `list` subcommand.
pub fn get_parser() -> Command {
    subcommand("list")
        .about("List all the virtual environments")
        .arg(
            optional("sort-by", "Properties to sort the output by")
                .value_parser([
                    "name",
                    "last_access",
                    "last-access",
                    "last_modification",
                    "last-modification",
                ])
                .default_value("name"),
        )
        .arg(flag("sort-reverse", "Display output in reverse order"))
}

/// Logic for executing the `list` subcommand.
pub fn execute(_ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement list")
}
