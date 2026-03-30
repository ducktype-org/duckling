use crate::{DuckCtx, QuackResult};
use clap::{ArgMatches, Command};

mod add;
mod build;
#[cfg(feature = "shell-completion")]
mod generate;
mod info;
mod init;
mod list;
mod publish;
mod remove;
mod repl;
mod run;
mod search;
mod sync;
mod tree;
mod unsync;

/// Get parsers for all the builtin subcommands.
pub fn subcommands() -> Vec<Command> {
    vec![
        add::get_parser(),
        build::get_parser(),
        #[cfg(feature = "shell-completion")]
        generate::get_parser(),
        info::get_parser(),
        init::get_parser(),
        repl::get_parser(),
        list::get_parser(),
        publish::get_parser(),
        remove::get_parser(),
        run::get_parser(),
        search::get_parser(),
        sync::get_parser(),
        tree::get_parser(),
        unsync::get_parser(),
    ]
}

/// Function signature which subcommands execution logic follows.
pub type ExecFn = fn(&DuckCtx, &ArgMatches) -> QuackResult<()>;

/// Get the [`ExecFn`] for the given subcommand name.
///
/// Returns [`None`], if `name` is not a valid duck builtin subcommand name.
pub fn exec_for(name: &str) -> Option<ExecFn> {
    let f = match name {
        "add" => add::execute,
        "build" => build::execute,
        #[cfg(feature = "shell-completion")]
        "generate" => generate::execute,
        "info" => info::execute,
        "init" => init::execute,
        "list" => list::execute,
        "publish" => publish::execute,
        "remove" => remove::execute,
        "repl" => repl::execute,
        "run" => run::execute,
        "search" => search::execute,
        "sync" => sync::execute,
        "tree" => tree::execute,
        "unsync" => unsync::execute,
        _ => return None,
    };
    Some(f)
}
