use std::{
    collections::HashMap,
    ffi::OsString,
    io::Write,
    path::{Path, PathBuf},
};

use crate::{
    DuckCtx, QuackResult, QuackResultContext, qp_bail,
    quackpack::core::compile::duckc::Duckc,
    util_common::{command_ext::CommandExt, path_ops_ext::PathOpsExt},
};
use clap::ArgMatches;
use tracing::debug;

use crate::duck::driver::{
    cli,
    cli_args_preprocessing::{aliases_expansion::expand_aliases, typos_fixing::fix_typos},
    cli_no_err,
    global_options::GlobalOptions,
    subcommands::exec_for,
};

/// Run the duck with the given [`DuckCtx`].
pub(crate) fn run(ctx: &mut DuckCtx) -> QuackResult<()> {
    let external = gather_external_subcmds(ctx);
    debug!(
        "found the external subcommands `{}`",
        external.keys().cloned().collect::<Vec<_>>().join(", ")
    );
    let cli = cli();

    if let Some(global_opts) = get_global_options() {
        global_opts.update_context(ctx);
    }

    let matches = cli.try_get_matches()?;
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
    debug!(
        "after expanding everything we have the subcommand: `{:#?}`",
        args.subcommand_name()
    );
    run_subcmd(ctx, args, &external)
}

/// Get [`GlobalOptions`] from the CLI arguments.
fn get_global_options() -> Option<GlobalOptions> {
    // We get matches without worrying about errors, only to retrieve GlobalCliOptions.
    // Later matching is done again on the real command, so any errors will be taken care of there.
    if let Ok(matches) = cli_no_err().try_get_matches() {
        GlobalOptions::from_matches(&matches).ok()
    } else {
        None
    }
}

/// Gather all known external subcommands.
///
/// In the returned map, keys are stripped from prefixes and suffixes; in other words, keys are
/// valid duck subcommands names.
fn gather_external_subcmds(ctx: &DuckCtx) -> HashMap<String, PathBuf> {
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
    ctx: &mut DuckCtx,
    args: ArgMatches,
    external: &HashMap<String, PathBuf>,
) -> QuackResult<()> {
    let Some((sub_cmd, sub_args)) = args.subcommand() else {
        // No subcommand provided, start REPL.
        return Duckc::start_repl_with(ctx).map(|_| ());
    };
    match (exec_for(sub_cmd), external.get(sub_cmd)) {
        (Some(exec_fn), Some(_)) => {
            ctx.error_console().warning(format!(
                "builtin subcommand `{sub_cmd}` shadows an external subcommand"
            ));
            exec_fn(ctx, sub_args)
        }
        (Some(exec_fn), None) => exec_fn(ctx, sub_args),
        (None, Some(exec_path)) => {
            drop(ctx.console().flush());
            drop(ctx.error_console().flush());
            let args = external_cli_args(sub_args);
            execute_external_subcmd(exec_path, args)
                .with_context(|| format!("failed to execute the external subcommand `{sub_cmd}`"))
        }
        (None, None) => qp_bail!("No such command: `{sub_cmd}`"),
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
