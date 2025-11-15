use crate::{DuckCtx, QuackResult};
use anyhow::bail;
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, subcommand};

pub fn get_parser() -> Command {
    subcommand("sync")
        .about("Synchronize current venv")
        .arg(flag("frozen", "Don't update freezefile"))
        .arg(flag("offline", "Don't perform network requests"))
        .arg(
            flag(
                "overwrite",
                "Overwrite any existing virtual environments with the same name",
            )
            .conflicts_with("global"),
        )
        .arg(
            flag("global", "Synchronize the global virtual environment")
                .conflicts_with("overwrite"),
        )
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement sync")
}
