use crate::{DuckCtx, QuackResult};
use clap::{ArgMatches, Command};

mod add;
mod build;
mod generate;
mod info;
mod init;
mod list;
mod publish;
mod remove;
mod run;
mod search;
mod sync;
mod tree;
mod unsync;

pub fn subcommands() -> Vec<Command> {
    vec![
        add::get_parser(),
        build::get_parser(),
        generate::get_parser(),
        info::get_parser(),
        init::get_parser(),
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

pub type ExecFn = fn(&DuckCtx, &ArgMatches) -> QuackResult<()>;

pub fn exec_for(name: &str) -> Option<ExecFn> {
    let f = match name {
        "add" => add::execute,
        "build" => build::execute,
        "generate" => generate::execute,
        "info" => info::execute,
        "init" => init::execute,
        "list" => list::execute,
        "publish" => publish::execute,
        "remove" => remove::execute,
        "run" => run::execute,
        "search" => search::execute,
        "sync" => sync::execute,
        "tree" => tree::execute,
        "unsync" => unsync::execute,
        _ => return None,
    };
    Some(f)
}
