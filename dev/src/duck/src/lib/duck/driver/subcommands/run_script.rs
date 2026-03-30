use std::{
    env::current_dir,
    ffi::OsString,
    path::{Path, PathBuf},
};

use crate::{
    DuckCtx, QuackError, QuackResult, QuackResultContext, StrId,
    duck::driver::cli_ext::flag,
    qp_bail,
    quackpack::subcommands::run_script::{RunScriptOptions, run_script},
};
use clap::{Arg, ArgMatches, Command, builder::ValueParser, value_parser};

use crate::duck::driver::cli_ext::{CommandExt, multi, subcommand};

pub const DUCKLING_SCRIPT_EXT: &str = "ds";

/// Creates parser for the `run_script` subcommand.
pub fn get_parser() -> Command {
    subcommand("run-script")
        .about("Run a Duckling script from the given path")
        .add_profile()
        .add_release()
        .add_jobs()
        .add_venv()
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
                .required(true),
        )
        .arg(
            multi("args", "Arguments passed to the script")
                .trailing_var_arg(true)
                .value_parser(value_parser!(OsString)),
        )
}

/// Logic for executing the `run_script` subcommand.
pub fn execute(ctx: &DuckCtx, matches: &ArgMatches) -> QuackResult<()> {
    let path = matches
        .try_get_one::<PathBuf>("path")?
        .context_internal("Path argument is required")?;
    let path: PathBuf = path.into();
    let path = current_dir()?.join(path);
    check_is_script(&path)?;
    let script_name = path
        .file_name()
        .context_internal("we assured that the path points to a file")?;
    let folder_path = path
        .parent()
        .context_internal("we assured that the path points to a file")?;
    let venv_id = matches.try_get_one::<String>("venv")?.map(StrId::new);
    run_script(RunScriptOptions {
        ctx,
        script_name,
        folder_path,
        venv_id,
        global: matches.get_flag("global"),
        overwrite: matches.get_flag("overwrite"),
        frozen: matches.get_flag("frozen"),
        strict_errors: matches.get_flag("external-errors"),
    })
}

/// Check if `path` points to a valid Duckling script.
fn check_is_script(path: &Path) -> QuackResult<()> {
    if !path.exists() {
        qp_bail!("the path {} does not exist", path.display());
    } else if !path.is_file() {
        qp_bail!("the path {} does not point to a file", path.display());
    } else if path.extension() != Some(&OsString::from(DUCKLING_SCRIPT_EXT)) {
        qp_bail!(
            QuackError::hint(format!(
                "the extension of Duckling scripts is `.{}`",
                DUCKLING_SCRIPT_EXT
            ))
            .context(format!(
                "the file at `{}` is not a Duckling script",
                path.display()
            ))
        );
    }
    Ok(())
}
