use crate::{
    DuckCtx, QuackResult, StrId,
    duck::driver::cli_ext::flag,
    quackpack::subcommands::activate::{ActivateOptions, activate},
};
use clap::{Arg, ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

/// Creates parser for the `info` subcommand.
pub fn get_parser() -> Command {
    subcommand("activate")
        .about("Activate a virtual environment")
        .arg(Arg::new("venv").help("Path to the Duckling script to run"))
        .arg(
            flag("global", "Activate the global venv")
                .required_unless_present("venv")
                .conflicts_with("venv"),
        )
}

/// Logic for executing the `info` subcommand.
pub fn execute(ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    let venv_id = matches.try_get_one::<String>("venv")?.map(StrId::new);
    activate(ActivateOptions {
        ctx,
        venv_id,
        global: matches.get_flag("global"),
    })
}
