use anyhow::bail;
use clap::{ArgMatches, Command};
use quackpack::QuackResult;

use crate::{
    DuckCtx,
    driver::cli_ext::{flag, optional, subcommand},
};

pub fn get_parser() -> Command {
    subcommand("list")
        .about("List all virtual environments")
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

pub fn execute(ctx: &DuckCtx, matches: ArgMatches) -> QuackResult<()> {
    bail!("implement list")
}
