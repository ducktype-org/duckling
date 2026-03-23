use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{Arg, ArgAction, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, optional, subcommand};

/// Creates parser for the `init` subcommand.
pub fn get_parser() -> Command {
    subcommand("init")
        .about("Initialize a new package")
        .arg(flag("venv", "Initialize a venv instead of a package").conflicts_with("full"))
        .arg(flag("full", "Initialize a full package, with prompts").conflicts_with("venv"))
        .arg(flag("ephemeral", "Mark the new venv as ephemeral").conflicts_with("local-storage"))
        .arg(
            flag(
                "local-storage",
                "Use a local package storage instead of the shared storage",
            )
            .conflicts_with("ephemeral"),
        )
        .arg(flag(
            "expose-freezefile",
            "Makes the synchronization export a freezefile and use the provided one",
        ))
        .arg(optional("name", "Override the package name"))
        .arg(
            Arg::new("path")
                .help("Path to the new package")
                .action(ArgAction::Set),
        )
}

/// Logic for executing the `init` subcommand.
pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement init")
}
