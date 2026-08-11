use crate::{
    DuckContext, QuackResult,
    quackpack::subcommands::remove::{RemoveOptions, remove},
};
use clap::{Arg, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, subcommand};

/// Creates parser for the `remove` subcommand.
pub fn get_parser() -> Command {
    subcommand("remove")
        .about("Remove the packages from the current venv")
        .arg(flag("global", "Remove the packages from the global venv instead").short('g'))
        .arg(flag("dev", "Remove a dev dependency instead"))
        .arg(
            Arg::new("name")
                .help("Name of the dependency to remove")
                .required(true),
        )
}

/// Logic for executing the `remove` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let name = matches
        .get_one::<String>("name")
        .expect("required by clap")
        .clone();
    let global = matches.get_flag("global");
    let dev_dep = matches.get_flag("dev");
    let options = RemoveOptions {
        name,
        global,
        dev_dep,
    };
    remove(ctx, options)
}
