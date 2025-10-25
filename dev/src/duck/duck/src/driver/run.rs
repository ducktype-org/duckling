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
        cli_no_help,
        global_cli_options::{Color, GlobalCliOptions},
        styles::get_styles,
        subcommands::exec_for,
    },
};
use anyhow::{Context, bail};
use clap::{ArgMatches, Command, error::ErrorKind::DisplayHelp};
use console::WithoutAnsi;
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

    let _matches = cli.try_get_matches();
    if let Err(e) = &_matches
        && e.kind() == DisplayHelp
    {
        return display_help(ctx);
    }
    let matches = _matches?;

    if let Some(chdir) = matches.get_one::<PathBuf>("directory") {
        std::env::set_current_dir(chdir)
            .with_context(|| format!("couldn't change CWD to `{}`", chdir.display()))?;
    }
    let global_opts = GlobalCliOptions::from_matches(&matches)?;
    global_opts.update_context(ctx);
    let args = fix_typos(matches, ctx, &external)?;
    let args = expand_aliases(args, ctx, &external, vec![])?;
    debug!(
        "after expanding everything we have subcommand: `{:#?}`",
        args.subcommand_name()
    );
    run_subcmd(ctx, args, &external)
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
        return display_help(ctx);
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

fn display_help(ctx: &mut DuckCtx) -> QuackResult<()> {
    // We apply matching to the duck command with disabled help, so that there is no early return on help by clap.
    // Then we get the true subcommand and manually print help for it
    //     (or for the whole command if there was no subcommand or it was not recognised).
    // We do this so that output for `duck --color never help` and similiar commands is not colored.

    let mut true_cli = cli();
    let no_help_matches = cli_no_help().try_get_matches()?;
    let global_opts = GlobalCliOptions::from_matches(&no_help_matches)?;
    global_opts.update_context(ctx);
    match no_help_matches.subcommand_name() {
        None => print_command_help(ctx, &mut true_cli, global_opts),
        Some(subcmd_name) => {
            if let Some(subcmd) = cli().find_subcommand_mut(subcmd_name) {
                // I do not understand why applying styles here again is necesseary, but it is.
                let mut subcmd = subcmd.clone().styles(get_styles());
                print_command_help(ctx, &mut subcmd, global_opts);
            } else {
                print_command_help(ctx, &mut true_cli, global_opts);
            }
        }
    }

    Ok(())
}

fn print_command_help(ctx: &DuckCtx, command: &mut Command, opts: GlobalCliOptions) {
    let help = command.render_help();
    match opts.color() {
        Color::Never => {
            ctx.console()
                .print_no_nl(WithoutAnsi::new(&help.to_string()));
        }
        _ => {
            ctx.console().print_no_nl(help.ansi());
        }
    }
}
