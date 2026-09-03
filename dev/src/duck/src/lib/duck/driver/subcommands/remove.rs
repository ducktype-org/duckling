use clap::{Arg, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, subcommand};
use crate::quackpack::core::DependencyKind;
use crate::quackpack::subcommands::remove::{RemoveOptions, remove};
use crate::{DuckContext, QuackResult};

/// Creates parser for the `remove` subcommand.
pub fn get_parser() -> Command {
    subcommand("remove")
        .about("Remove a dependency from the current venv")
        .arg(flag("global", "Remove a dependency from the global venv instead").short('g'))
        .arg(flag("dev", "Remove a dev dependency instead"))
        .arg(
            Arg::new("name")
                .help("Name of the dependency to remove")
                .required(true),
        )
}

/// Logic for executing the `remove` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let name = matches.get_one::<String>("name").expect("required by clap");
    let global = matches.get_flag("global");
    let kind = determine_kind(matches);
    let options = RemoveOptions { name, global, kind };
    remove(ctx, options)
}

/// Determine the kind of the dependency to remove.
fn determine_kind(matches: &ArgMatches) -> DependencyKind {
    if matches.get_flag("dev") {
        DependencyKind::Dev
    } else {
        DependencyKind::Normal
    }
}
