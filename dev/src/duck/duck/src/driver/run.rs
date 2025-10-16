use std::{
    collections::HashMap,
    ffi::OsString,
    os::unix::process::CommandExt,
    path::{Path, PathBuf},
    process::exit,
};

use crate::{
    DuckCtx,
    driver::{
        cli,
        cli_args_preprocessing::{aliases_expansion::expand_aliases, typos_fixing::fix_typos},
        global_cli_options::GlobalCliOptions,
        subcommands::exec_for,
    },
};
use anyhow::{Context, bail};
use clap::ArgMatches;
use is_executable::is_executable;
use quackpack::QuackResult;
use tracing::debug;

pub(crate) fn run(ctx: &mut DuckCtx) -> QuackResult<()> {
    let external = gather_external_subcmds(ctx);
    let cli = cli();
    let matches = cli.try_get_matches()?;
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
    const PREFIX: &str = "qp-";
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
            if is_executable(executable.as_path()) {
                commands.insert(String::from(stripped), executable);
            }
        }
    }
    commands
}

fn run_subcmd(
    ctx: &DuckCtx,
    args: ArgMatches,
    external: &HashMap<String, PathBuf>,
) -> QuackResult<()> {
    let Some((sub_cmd, sub_args)) = args.subcommand() else {
        // No subcommand provided.
        print_parser_help(ctx);
        return Ok(());
    };
    if let Some(exec_fn) = exec_for(sub_cmd) {
        // Internal subcommand.
        exec_fn(ctx, sub_args.to_owned())
    } else if let Some(exec_path) = external.get(sub_cmd) {
        // External subcommand.
        let cli_args = external_cli_args(sub_cmd, sub_args);
        execute_external_subcmd(exec_path, cli_args)
    } else {
        // Unrecognizable subcommand.
        bail!("No such command: `{}`", sub_cmd);
    }
}

fn external_cli_args(sub_cmd: &str, sub_args: &ArgMatches) -> Vec<OsString> {
    let mut cli_arguments = vec![OsString::from(sub_cmd)];
    cli_arguments.extend(
        sub_args
            .get_many::<OsString>("")
            .unwrap_or_default()
            .cloned(),
    );
    cli_arguments
}

fn execute_external_subcmd(exec_path: &Path, cli_args: Vec<OsString>) -> QuackResult<()> {
    let Some(exec_path_str) = exec_path.as_os_str().to_str() else {
        bail!("Could not decode external subcommand path.");
    };
    let mut command = std::process::Command::new(exec_path_str);
    command.args(cli_args);
    if cfg!(unix) {
        // If nothing goes wrong exec does not return.
        let err: anyhow::Error = command.exec().into();
        Err(err)
    } else {
        let mut child = command.spawn()?;
        let child_exit_status = child.wait()?;
        match child_exit_status.code() {
            Some(n) => exit(n),

            // The child process was interrupted
            None => Ok(()),
        }
    }
}

fn print_parser_help(ctx: &DuckCtx) {
    let mut parser = cli();
    let help = parser.render_help();
    ctx.console().print_no_nl(help.ansi());
}
