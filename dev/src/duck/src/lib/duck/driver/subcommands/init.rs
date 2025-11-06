use crate::QuackResult;
use anyhow::bail;
use clap::{Arg, ArgAction, ArgMatches, Command};

use crate::{
    DuckCtx,
    duck::driver::cli_ext::{flag, optional, subcommand},
};

pub fn get_parser() -> Command {
    subcommand("init")
        .about("Initialize a new package")
        .arg(flag("venv", "Initialize venv instead of a package").conflicts_with("full"))
        .arg(flag("full", "Initialize full package, with prompts").conflicts_with("venv"))
        .arg(flag("ephemeral", "Mark new venv as ephemeral").conflicts_with("local-storage"))
        .arg(
            flag(
                "local-storage",
                "Use local package storage instead of shared storage",
            )
            .conflicts_with("ephemeral"),
        )
        .arg(flag(
            "expose-freezefile",
            "Makes synchronization export a freezefile and use the provided one",
        ))
        .arg(optional("name", "Override package name"))
        .arg(
            Arg::new("path")
                .help("Path to the new package")
                .action(ArgAction::Set),
        )
}

pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    bail!("implement init")
}
