use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, optional, subcommand};
use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::quackpack::subcommands::list::{ListOptions, VenvOrderings, list};
use crate::util::error::HintMessage;
use crate::{DuckContext, QuackResult, QuackResultContext};

/// Creates parser for the `list` subcommand.
pub fn get_parser() -> Command {
    subcommand("list")
        .about("Lists all the virtual environments in the storage")
        .arg(
            optional("sort-by", "Properties to sort the output by")
                .value_parser(["name", "previous-access", "last-modification"])
                .default_value("name"),
        )
        .arg(flag("sort-reverse", "Display output in reverse order"))
        .arg(flag(
            "global-storage",
            "Display all the venvs in the global storage",
        ))
}

/// Logic for executing the `list` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let storage_path = if matches.get_flag("global-storage") {
        ctx.default_storage_root().into_not_locked_path()
    } else {
        let pcx = PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No).with_context(|| {
            HintMessage::new("to display the venvs in the global storage use `global-storage` flag")
        })?;
        pcx.storage_path()?
    };
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
        storage_path,
    };
    list(options)
}
