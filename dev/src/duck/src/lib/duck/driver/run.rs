use std::collections::HashMap;
use std::ffi::{OsStr, OsString};
use std::path::{Path, PathBuf};

use clap::ArgMatches;
use tracing::debug;

use super::cli;
use super::cli_args_preprocessing::aliases_expansion::expand_aliases;
use super::cli_args_preprocessing::typos_fixing::fix_typos;
use super::global_options::GlobalOptions;
use super::subcommands::exec_for;
use super::subcommands::run_script::{check_is_script, possible_script_path_subcmd};
use crate::duck::driver::cli_ext::ArgMatchesExt;
use crate::duck::driver::external_subcommands::ExternalSubcommands;
use crate::quackpack::core::compile::duckc::Duckc;
use crate::quackpack::subcommands::run_script::{RunScriptOptions, run_script};
use crate::util::command_ext::CommandExt;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail};

/// Run the duck with the given [`DuckContext`].
pub(crate) fn run(ctx: &mut DuckContext) -> QuackResult<()> {
    let external = ExternalSubcommands::default();
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
    let args = expand_aliases(args, ctx, vec![])?;
    global_opts.update_with_subcommand_matches(&args);
    if args.safe_get_flag("version") {
        let version = crate::duck::version::Version::get();
        version.print(ctx, global_opts.verbose)?;
        return Ok(());
    }
    global_opts.update_context(ctx)?;
    debug!(
        subcommand = ?args.subcommand_name(),
        "after expanding everything"
    );
    run_subcmd(ctx, args, &external)
}

/// Gather all known external subcommands.
///
/// In the returned map, keys are stripped from prefixes and suffixes; in other words, keys are
/// valid duck subcommands names.
pub(super) fn gather_external_subcmds(ctx: &DuckContext) -> HashMap<String, PathBuf> {
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
            let name = entry.file_name();
            let Some(name) = name.to_str() else {
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

    debug!(
        external = ?commands.keys(),
        "found the external subcommands"
    );
    commands
}

/// Execute fully fixed, parsed, and expanded subcommand.
fn run_subcmd(
    ctx: &DuckContext,
    args: ArgMatches,
    external: &ExternalSubcommands,
) -> QuackResult<()> {
    let Some((name, args)) = args.subcommand() else {
        // No subcommand provided, start REPL.
        return Duckc::start_repl_with(ctx).map(|_| ());
    };
    if let Some(exec_fn) = exec_for(name) {
        return exec_fn(ctx, args);
    }
    if let Some(exec_path) = external.load(ctx).get(name) {
        let args = external_cli_args(args);
        return execute_external_subcmd(exec_path, args)
            .with_context(|| format!("failed to execute the external subcommand `{name}`"));
    }
    if let Some(path) = possible_script_path_subcmd(args) {
        let path = Path::new(path);
        let path = path.resolve_with_tilde(ctx);
        check_is_script(&path)?;
        let args = external_cli_args(args);
        let opts = RunScriptOptions::from_path_and_args_with_defaults(ctx, &path, args)?;
        return run_script(opts);
    }
    qp_bail!("no such command as `{name}`")
}

/// Get all arguments passed to the external subcommand.
fn external_cli_args(sub_args: &ArgMatches) -> Vec<&OsStr> {
    sub_args
        .get_many::<OsString>("")
        .unwrap_or_default()
        .map(OsString::as_os_str)
        .collect()
}

/// Execute the external subcommand.
fn execute_external_subcmd(exec_path: &Path, cli_args: Vec<&OsStr>) -> QuackResult<()> {
    debug!(
        path = %exec_path.display(),
        ?cli_args,
        "executing external command"
    );
    let mut command = std::process::Command::new(exec_path);
    command.args(cli_args);
    command.exec_replace().map(|_| ())
}
