use std::{collections::HashMap, path::PathBuf};

use crate::{
    DuckCtx,
    driver::{
        cli,
        cli_args_preprocessing::{aliases_expansion::expand_aliases, typos_fixing::fix_typos},
        global_cli_options::GlobalCliOptions,
    },
};
use anyhow::Context;
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
    Ok(())
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

fn print_parser_help(ctx: &DuckCtx) {
    let mut parser = cli();
    let help = parser.render_help();
    ctx.console().print_no_nl(help.ansi());
}
