use std::collections::HashMap;
use std::ffi::OsString;
use std::io::Write;
use std::path::{Path, PathBuf};

use clap::ArgMatches;
use tracing::debug;

use crate::duck::driver::cli;
use crate::duck::driver::cli_args_preprocessing::aliases_expansion::expand_aliases;
use crate::duck::driver::cli_args_preprocessing::typos_fixing::fix_typos;
use crate::duck::driver::global_options::GlobalOptions;
use crate::duck::driver::subcommands::exec_for;
use crate::duck::driver::subcommands::run_script::{check_is_script, possible_script_path_subcmd};
use crate::quackpack::core::compile::duckc::Duckc;
use crate::quackpack::subcommands::run_script::{RunScriptOptions, run_script};
use crate::util::command_ext::CommandExt;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail};

/// Run the duck with the given [`DuckContext`].
pub(crate) fn run(ctx: &mut DuckContext) -> QuackResult<()> {
    let external = gather_external_subcmds(ctx);
    debug!(
        "found the external subcommands `{}`",
        external.keys().cloned().collect::<Vec<_>>().join(", ")
    );
    let cli = cli();

    let matches = cli.try_get_matches()?;
    let mut global_opts = GlobalOptions::from_matches(&matches);

    if let Some(chdir) = matches.get_one::<PathBuf>("directory") {
        std::env::set_current_dir(chdir).with_context(|| {
            format!(
                "couldn't change the current working directory to `{}`",
                chdir.display()
            )
        })?;
        ctx.reload_cwd()?;
    }
    let args = fix_typos(matches, ctx, &external)?;
    let args = expand_aliases(args, ctx, &external, vec![])?;
    global_opts.update_with_subcommand_matches(&args);
    global_opts.update_context(ctx)?;
    debug!(
        "after expanding everything we have the subcommand: `{:#?}`",
        args.subcommand_name()
    );
    run_subcmd(ctx, args, &external)
}

/// Gather all known external subcommands.
///
/// In the returned map, keys are stripped from prefixes and suffixes; in other words, keys are
/// valid duck subcommands names.
fn gather_external_subcmds(ctx: &DuckContext) -> HashMap<String, PathBuf> {
    use std::env;
    const PREFIX: &str = "duck-";
    const SUFFIX: &str = env::consts::EXE_SUFFIX;
    let Some(path) = ctx.env().get_os("PATH") else {
        return HashMap::new();
    };
    let mut commands = HashMap::new();
    for segment in env::split_paths(path) {
        let Ok(dir) = segment.read_dir() else {
            continue;
        };
        for entry in dir.filter_map(|x| x.ok()) {
            let executable = entry.path();
            let name_ = entry.file_name();
            let Some(name) = name_.to_str() else {
                continue;
            };
            let Some(stripped) = name
                .strip_prefix(PREFIX)
                .and_then(|x| x.strip_suffix(SUFFIX))
            else {
                continue;
            };
            if executable.as_path().is_executable() {
                commands.insert(String::from(stripped), executable);
            }
        }
    }
    commands
}

/// Execute fully fixed, parsed, and expanded subcommand.
fn run_subcmd(
    ctx: &mut DuckContext,
    args: ArgMatches,
    external: &HashMap<String, PathBuf>,
) -> QuackResult<()> {
    let Some((sub_cmd, sub_args)) = args.subcommand() else {
        // No subcommand provided, start REPL.
        return Duckc::start_repl_with(ctx).map(|_| ());
    };
    match (
        exec_for(sub_cmd),
        external.get(sub_cmd),
        possible_script_path_subcmd(&args),
    ) {
        (Some(exec_fn), Some(_), _) => {
            ctx.error_console().warning(format!(
                "builtin subcommand `{sub_cmd}` shadows an external subcommand"
            ))?;
            exec_fn(ctx, sub_args)
        }
        (Some(exec_fn), None, _) => exec_fn(ctx, sub_args),
        (None, Some(exec_path), Some(_)) => {
            ctx.console().note(format!(
                "external subcommand {sub_cmd} possibly shadows a script"
            ))?;
            ctx.console().hint(format!(
                "If you would like to run a script with that name, type `duck ./{sub_cmd}`"
            ))?;
            drop(ctx.console().flush());
            drop(ctx.error_console().flush());
            let args = external_cli_args(sub_args);
            execute_external_subcmd(exec_path, args)
                .with_context(|| format!("failed to execute the external subcommand `{sub_cmd}`"))
        }
        (None, Some(exec_path), None) => {
            drop(ctx.console().flush());
            drop(ctx.error_console().flush());
            let args = external_cli_args(sub_args);
            execute_external_subcmd(exec_path, args)
                .with_context(|| format!("failed to execute the external subcommand `{sub_cmd}`"))
        }
        (None, None, Some(path)) => {
            let path = ctx.cwd().join(path);
            check_is_script(&path)?;
            let args = external_cli_args(sub_args);
            run_script(RunScriptOptions::from_path_and_args_with_defaults(
                ctx, &path, args,
            )?)
        }
        (None, None, None) => qp_bail!("No such command: `{sub_cmd}`"),
    }
}

/// Get all arguments passed to the external subcommand.
fn external_cli_args(sub_args: &ArgMatches) -> Vec<OsString> {
    sub_args
        .get_many::<OsString>("")
        .unwrap_or_default()
        .cloned()
        .collect::<Vec<_>>()
}

/// Execute the external subcommand.
fn execute_external_subcmd(exec_path: &Path, cli_args: Vec<OsString>) -> QuackResult<()> {
    debug!(
        "executing the external command `{}`, arguments are `{cli_args:?}`",
        exec_path.display()
    );
    let mut command = std::process::Command::new(exec_path);
    command.args(cli_args);
    command.exec_replace().map(|_| ())
}
