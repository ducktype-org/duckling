use std::ffi::OsString;

use crate::{DuckCtx, QuackResult, qp_bail};
use clap::{ArgMatches, Command, value_parser};

use crate::duck::driver::cli_ext::{CommandExt, flag, multi, subcommand};

/// Creates parser for the `run` subcommand.
pub fn get_parser() -> Command {
    subcommand("run")
        .about("Build the current package and run it")
        .add_profile()
        .add_release()
        .add_features_conflicting(
            "Build the current package with these features",
            "all-features",
        )
        .arg(
            flag(
                "all-features",
                "Build the current package with all possible features",
            )
            .conflicts_with("features"),
        )
        .add_jobs()
        .arg(
            multi("args", "Arguments passed to the compiled binary")
                .trailing_var_arg(true)
                .value_parser(value_parser!(OsString)),
        )
}

/// Logic for executing the `run` subcommand.
pub fn execute(_ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    qp_bail!("implement run")
}
