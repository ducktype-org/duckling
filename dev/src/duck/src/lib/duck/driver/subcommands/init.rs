use std::path::PathBuf;

use clap::builder::ValueParser;
use clap::{Arg, ArgAction, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, optional, subcommand};
use crate::quackpack::subcommands::init::{InitOptions, init};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult};

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
        .arg(flag(
            "git",
            "initialize a git repository in the projects root",
        ))
        .arg(optional("name", "Override the package name"))
        .arg(
            Arg::new("path")
                .help("Path to the new package")
                .value_parser(ValueParser::path_buf())
                .required(true)
                .action(ArgAction::Set),
        )
}

/// Logic for executing the `init` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let at = matches
        .get_one::<PathBuf>("path")
        .expect("required by clap");
    let at = at.resolve_with_tilde(ctx);
    let explicit_name = matches.get_one::<String>("name").map(String::as_str);
    init(InitOptions {
        ctx,
        at,
        explicit_name,
        as_venv: matches.get_flag("venv"),
        expose_freezefile: matches.get_flag("expose-freezefile"),
        local_storage: matches.get_flag("local-storage"),
        ephemeral: matches.get_flag("ephemeral"),
        git: matches.get_flag("git"),
        full: matches.get_flag("full"),
    })
}
