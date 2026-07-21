use clap::{ArgMatches, Command};

use crate::{DuckContext, QuackResult};

// @TODO: #1650 Restore removed subcommands once they are implemented.
mod build;
mod clean_storage;
#[cfg(feature = "shell-completion")]
mod generate;
mod info;
mod init;
mod list;
mod repl;
mod run;
pub mod run_script;
mod sync;

/// Get parsers for all the builtin subcommands.
pub fn subcommands() -> Vec<Command> {
    vec![
        build::get_parser(),
        run::get_parser(),
        #[cfg(feature = "shell-completion")]
        generate::get_parser(),
        init::get_parser(),
        run_script::get_parser(),
        sync::get_parser(),
        repl::get_parser(),
        list::get_parser(),
        info::get_parser(),
        clean_storage::get_parser(),
    ]
}

/// Function signature which subcommands execution logic follows.
pub type ExecFn = fn(&DuckContext, &ArgMatches) -> QuackResult<()>;

/// Get the [`ExecFn`] for the given subcommand name.
///
/// Returns [`None`], if `name` is not a valid duck builtin subcommand name.
pub fn exec_for(name: &str) -> Option<ExecFn> {
    let f = match name {
        "build" => build::execute,
        "run" => run::execute,
        #[cfg(feature = "shell-completion")]
        "generate" => generate::execute,
        "init" => init::execute,
        "run-script" => run_script::execute,
        "sync" => sync::execute,
        "repl" => repl::execute,
        "list" => list::execute,
        "info" => info::execute,
        "clean-storage" => clean_storage::execute,
        _ => return None,
    };
    Some(f)
}
