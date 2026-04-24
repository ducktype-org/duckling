use std::path::PathBuf;

use clap::builder::ValueParser;
use clap::{Arg, ArgAction, ArgMatches, Command};

use crate::duck::driver::cli_ext::{flag, optional, subcommand};
use crate::quackpack::subcommands::init::{InitOptions, init};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, qp_bail_internal};

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
                .value_parser(ValueParser::path_buf())
                .action(ArgAction::Set),
        )
}

/// Logic for executing the `init` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let unsupported_flags = [
        "venv",
        "full",
        "ephemeral",
        "local-storage",
        "expose-freezefile",
    ];
    for flag in unsupported_flags {
        bail_on_unsupported_flag(matches.get_flag(flag), flag)?;
    }
    let at = match matches.get_one::<PathBuf>("path") {
        Some(at) => at.resolve()?,
        None => ctx.cwd().to_path_buf(),
    };
    let name = match matches.get_one::<String>("name") {
        Some(name) => name.into(),
        None => at.file_name().expect("file without filename").into(),
    };
    init(InitOptions { ctx, at, name })
}

/// Return an internal error for unsupported flags.
fn bail_on_unsupported_flag(flag: bool, name: &str) -> QuackResult<()> {
    if flag {
        qp_bail_internal!("init flag `--{name}` is not yet supported")
    }
    Ok(())
}
