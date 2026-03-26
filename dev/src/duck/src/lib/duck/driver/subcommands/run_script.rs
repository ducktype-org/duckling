use std::{ffi::OsString, path::PathBuf};

use crate::{
    DuckCtx, QuackResult, QuackResultContext, quackpack::subcommands::run_script::run_script,
};
use clap::{Arg, ArgMatches, Command, value_parser};

use crate::duck::driver::cli_ext::{CommandExt, multi, subcommand};

/// Creates parser for the `run_script` subcommand.
pub fn get_parser() -> Command {
    subcommand("run_script")
        .about("Run a Duckling script from the given path")
        .add_profile()
        .add_release()
        .add_jobs()
        .arg(Arg::new("path").help("Path to the Duckling script to run"))
        .arg(
            multi("args", "Arguments passed to the compiled binary")
                .trailing_var_arg(true)
                .value_parser(value_parser!(OsString)),
        )
}

/// Logic for executing the `run_script` subcommand.
pub fn execute(_ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    let path = matches
        .try_get_raw("path")?
        .context("Please specify the path to the script to run")?
        .next()
        .context_internal("We assured that path was specified")?;
    let path: PathBuf = path.into();
    run_script(path)
}
