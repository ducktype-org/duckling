use std::ffi::{OsStr, OsString};
use std::path::{Path, PathBuf};

use clap::builder::ValueParser;
use clap::{Arg, ArgMatches, Command, ValueHint, value_parser};

use crate::duck::driver::cli_ext::{CommandExt, flag, subcommand};
use crate::quackpack::subcommands::run_script::{RunScriptOptions, run_script};
use crate::util::error::MessageError;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackError, QuackResult, qp_bail};

pub const DUCKLING_SCRIPT_EXT: &str = "dks";

/// Creates parser for the `run_script` subcommand.
pub fn get_parser() -> Command {
    subcommand("run-script")
        .about("Run a Duckling script from the given path")
        .add_profile()
        .add_release()
        .add_jobs()
        .arg(flag("frozen", "Don't update the freezefile"))
        .arg(
            flag(
                "overwrite",
                "Overwrite any existing virtual environments with the same name",
            )
            .conflicts_with("global"),
        )
        .arg(
            flag("global", "Run the script in the global virtual environment")
                .conflicts_with("overwrite"),
        )
        .arg(flag(
            "external-errors",
            "Halt computation after encountering errors in foreign manifests",
        ))
        .arg(
            Arg::new("path")
                .help("Path to the Duckling script to run")
                .value_parser(ValueParser::path_buf())
                .value_hint(ValueHint::FilePath)
                .required(true),
        )
        .arg(
            Arg::new("args")
                .help("Arguments passed to the script")
                .trailing_var_arg(true)
                .num_args(0..)
                .value_parser(value_parser!(OsString)),
        )
}

/// Logic for executing the `run_script` subcommand.
pub fn execute(ctx: &DuckContext, matches: &ArgMatches) -> QuackResult<()> {
    let path = matches
        .get_one::<PathBuf>("path")
        .expect("guarded by the parser");
    let path = path.resolve_with_tilde(ctx);
    check_is_script(&path)?;
    run_script(RunScriptOptions::from_path_and_matches(
        ctx, &path, matches,
    )?)
}

/// Check if `path` points to a valid Duckling script.
pub fn check_is_script(path: &Path) -> QuackResult<()> {
    if !path.exists() {
        qp_bail!("the script path `{}` does not exist", path.display());
    }
    if !path.is_file() {
        qp_bail!(
            "the script path `{}` does not point to a file",
            path.display()
        );
    }
    if path.extension() != Some(OsStr::new(DUCKLING_SCRIPT_EXT)) {
        qp_bail!(
            QuackError::hint(format!(
                "the extension of Duckling scripts is `.{}`",
                DUCKLING_SCRIPT_EXT
            ))
            .context(MessageError(
                format!("the file at `{}` is not a Duckling script", path.display()).into()
            ))
        );
    }
    Ok(())
}

/// Guess whether name could be a path to a script to run.
pub fn is_name_possible_script_path_subcmd(name: &str) -> bool {
    let path = Path::new(name);
    path.extension().is_some() || path.components().count() > 1
}
