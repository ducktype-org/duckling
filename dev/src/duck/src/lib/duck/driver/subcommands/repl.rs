use crate::{DuckContext, QuackResult, quackpack::core::compile::duckc::Duckc};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;

/// Creates parser for the `repl` subcommand.
pub fn get_parser() -> Command {
    subcommand("repl").about("Start a REPL session")
}

/// Logic for executing the `repl` subcommand.
pub fn execute(ctx: &DuckContext, _matches: &ArgMatches) -> QuackResult<()> {
    Duckc::start_repl_with(ctx).map(|_| ())
}
