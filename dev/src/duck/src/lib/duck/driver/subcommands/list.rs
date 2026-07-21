use std::path::PathBuf;

use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, optional, subcommand};
use crate::quackpack::subcommands::list::{ListOptions, VenvOrderings, list};
use crate::{DuckContext, QuackResult};

/// Creates parser for the `list` subcommand.
pub fn get_parser() -> Command {
    subcommand("list")
        .about("List all the virtual environments")
        .arg(
            optional("sort-by", "Properties to sort the output by")
                .value_parser(["name", "previous-access", "last-modification"])
                .default_value("name"),
        )
        .arg(flag("sort-reverse", "Display output in reverse order"))
        .arg(optional(
            "storage",
            "Path to the storage from which to list venvs",
        ))
}

/// Logic for executing the `list` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let storage: PathBuf = matches
        .get_one::<PathBuf>("storage")
        .cloned()
        .unwrap_or(ctx.duck_home().storage().into_not_locked_path());
    let output_ordering = match matches.get_one::<String>("sort-by").unwrap().as_str() {
        "name" => VenvOrderings::Name,
        "previous-access" => VenvOrderings::Access,
        "last-modification" => VenvOrderings::Modification,
        _ => panic!("guarded by the parser"),
    };
    let reverse_order = matches.get_flag("sort-reverse");
    let options = ListOptions {
        ctx,
        output_ordering,
        reverse_order,
        storage,
    };
    list(options)
}
