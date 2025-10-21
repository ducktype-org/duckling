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
use clap::{ArgMatches, Command, error::ErrorKind};
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
    let matches_ = cli.try_get_matches();
    if let Err(err) = &matches_
        && err.kind() == ErrorKind::DisplayHelp
    {
        return run_help(ctx);
    }
    let matches = matches_?;

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
        return run_help(ctx);
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
    Err(command.exec_replace().into())
}

fn run_help(ctx: &mut DuckCtx) -> QuackResult<()> {
    let cli_nh = cli_no_help();
    let mut cli_real = cli();
    let matches = cli_nh.try_get_matches()?;
    let global_opts = GlobalCliOptions::from_matches(&matches)?;
    global_opts.update_context(ctx);
    let name = matches.subcommand_name();
    match name {
        None => match global_opts.color() {
            Color::Never => print_parser_help(ctx, &mut cli_real, true),
            _ => print_parser_help(ctx, &mut cli_real, false),
        },
        Some(subcmd_name) => {
            let mut found_subcmd = false;
            for subcmd in cli().get_subcommands_mut() {
                let mut subcmd_ = subcmd.clone().styles(get_styles());
                if subcmd.get_name() == subcmd_name {
                    found_subcmd = true;
                    match global_opts.color() {
                        Color::Never => print_parser_help(ctx, &mut subcmd_, true),
                        _ => print_parser_help(ctx, &mut subcmd_, false),
                    }
                }
            }
            if !found_subcmd {
                match global_opts.color() {
                    Color::Never => print_parser_help(ctx, &mut cli_real, true),
                    _ => print_parser_help(ctx, &mut cli_real, false),
                }
            }
        }
    }

    Ok(())
}

fn print_parser_help(ctx: &DuckCtx, parser: &mut Command, colors_disabled: bool) {
    let help = parser.render_help();
    if colors_disabled {
        ctx.console()
            .print_no_nl(WithoutAnsi::new(&help.to_string()));
    } else {
        ctx.console().print_no_nl(help.ansi());
    }
}
