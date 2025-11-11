use std::{
    collections::HashMap,
    ffi::OsString,
    path::{Path, PathBuf},
};

use crate::{
    DuckCtx,
    driver::{
        cli,
        cli_args_preprocessing::{aliases_expansion::expand_aliases, typos_fixing::fix_typos},
        cli_no_err,
        global_cli_options::GlobalCliOptions,
        subcommands::exec_for,
    },
};
use anyhow::{Context, bail};
use clap::ArgMatches;
use quackpack::QuackResult;
use rustvil::{fs::PathExt, os::CommandExt};
use tracing::debug;

pub(crate) fn run(ctx: &mut DuckCtx) -> QuackResult<()> {
    let external = gather_external_subcmds(ctx);
    debug!(
        "found external subcommands `{}`",
        external.keys().cloned().collect::<Vec<_>>().join(", ")
    );
    let cli = cli();

    if let Some(global_opts) = get_global_options() {
        global_opts.update_context(ctx);
    }

    let matches = cli.try_get_matches()?;
    if let Some(chdir) = matches.get_one::<PathBuf>("directory") {
        std::env::set_current_dir(chdir)
            .with_context(|| format!("couldn't change CWD to `{}`", chdir.display()))?;
    }
    let args = fix_typos(matches, ctx, &external)?;
    let args = expand_aliases(args, ctx, &external, vec![])?;
    debug!(
        "after expanding everything we have subcommand: `{:#?}`",
        args.subcommand_name()
    );
    run_subcmd(ctx, args, &external)
}

fn get_global_options() -> Option<GlobalCliOptions> {
    // We get matches without worrying about errors, only to retrieve GlobalCliOptions.
    // Later matching is done again on the real command, so any errors will be taken care of there.
    if let Ok(matches) = cli_no_err().try_get_matches() {
        GlobalCliOptions::from_matches(&matches).ok()
    } else {
        None
    }
}

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

fn run_subcmd(
    ctx: &mut DuckCtx,
    args: ArgMatches,
    external: &HashMap<String, PathBuf>,
) -> QuackResult<()> {
    let Some((sub_cmd, sub_args)) = args.subcommand() else {
        // No subcommand provided.
        ctx.console().print_no_nl(cli().render_help().ansi());
        return Ok(());
    };
    match (exec_for(sub_cmd), external.get(sub_cmd)) {
        (Some(exec_fn), Some(_)) => {
            ctx.error_console().warning(format!(
                "builtin subcommand `{sub_cmd}` shadows external subcmd"
            ));
            exec_fn(ctx, sub_args)
        }
        (Some(exec_fn), None) => exec_fn(ctx, sub_args),
        (None, Some(exec_path)) => {
            drop(ctx.console().flush());
            drop(ctx.error_console().flush());
            let args = external_cli_args(sub_args);
            execute_external_subcmd(exec_path, args)
                .with_context(|| format!("failed to execute external subcmd `{sub_cmd}`"))
        }
        (None, None) => bail!("No such command: `{sub_cmd}`"),
    }
}

fn external_cli_args(sub_args: &ArgMatches) -> Vec<OsString> {
    sub_args
        .get_many::<OsString>("")
        .unwrap_or_default()
        .cloned()
        .collect::<Vec<_>>()
}

fn execute_external_subcmd(exec_path: &Path, cli_args: Vec<OsString>) -> QuackResult<()> {
    debug!(
        "executing external cmd `{}`, args are `{cli_args:?}`",
        exec_path.display()
    );
    let mut command = std::process::Command::new(exec_path);
    command.args(cli_args);
    command.exec_replace().map(|_| ()).map_err(|x| x.into())
}
