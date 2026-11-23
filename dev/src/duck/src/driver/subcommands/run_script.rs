// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::ffi::{OsStr, OsString};
use std::path::{Path, PathBuf};

use clap::builder::ValueParser;
use clap::{Arg, ArgMatches, Command, ValueHint, value_parser};
use quackpack::core::compile::profiles::DEFAULT_SCRIPT_PROFILE_NAME;
use quackpack::subcommands::run_script::{RunScriptOptions, run_script};
use tracing::debug;

use crate::cli::{flag, subcommand};
use crate::driver::cli_ext::{CommandExt, jobs_from_matches};
use crate::util::duckling_consts::DUCKLING_SCRIPT_EXT;
use crate::util::filesystem::resolve_with_tilde;
use crate::{DuckContext, QuackError, QuackResult, qp_bail};

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
    let path = resolve_with_tilde(path, ctx);
    check_is_script(&path)?;
    run_script(options_from_matches(ctx, &path, matches))
}

/// Create [`RunScriptOptions`] for the script at `path` from the parsed `matches`.
fn options_from_matches<'duck, 'matches>(
    ctx: &'duck DuckContext,
    path: &'duck Path,
    matches: &'matches ArgMatches,
) -> RunScriptOptions<'duck, 'matches> {
    let profile = matches
        .get_one::<String>("profile")
        .map(String::as_str)
        .unwrap_or(DEFAULT_SCRIPT_PROFILE_NAME)
        .into();
    let args: Vec<&OsStr> = matches
        .get_many::<OsString>("args")
        .unwrap_or_default()
        .map(OsString::as_os_str)
        .collect();
    debug!(?args);
    RunScriptOptions {
        ctx,
        path,
        global: matches.get_flag("global"),
        profile,
        overwrite: matches.get_flag("overwrite"),
        frozen: matches.get_flag("frozen"),
        strict_errors: matches.get_flag("external-errors"),
        args,
        jobs: jobs_from_matches(matches),
    }
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
        let mut err = QuackError::message(format!(
            "the file at `{}` is not a Duckling script",
            path.display()
        ));
        err.add_hint(format!(
            "the extension of Duckling scripts is `.{}`",
            DUCKLING_SCRIPT_EXT
        ));
        return Err(err);
    }
    Ok(())
}

/// Guess whether name could be a path to a script to run.
pub fn is_name_possible_script_path_subcmd(name: &str) -> bool {
    let path = Path::new(name);
    path.extension().is_some() || path.components().count() > 1
}
