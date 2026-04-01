use crate::{DuckCtx, QuackResult};
use clap::{ArgMatches, Command};

// @TODO: #1650 Restore removed subcommands once they are implemented.
mod build;
#[cfg(feature = "shell-completion")]
mod generate;
mod init;
mod repl;

/// Get parsers for all the builtin subcommands.
pub fn subcommands() -> Vec<Command> {
    vec![
        build::get_parser(),
        #[cfg(feature = "shell-completion")]
        generate::get_parser(),
        init::get_parser(),
        repl::get_parser(),
    ]
}

/// Function signature which subcommands execution logic follows.
pub type ExecFn = fn(&DuckCtx, &ArgMatches) -> QuackResult<()>;

/// Get the [`ExecFn`] for the given subcommand name.
///
/// Returns [`None`], if `name` is not a valid duck builtin subcommand name.
pub fn exec_for(name: &str) -> Option<ExecFn> {
    let f = match name {
        "build" => build::execute,
        #[cfg(feature = "shell-completion")]
        "generate" => generate::execute,
        "init" => init::execute,
        "repl" => repl::execute,
        _ => return None,
    };
    Some(f)
}
