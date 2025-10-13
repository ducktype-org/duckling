use std::{collections::HashMap, path::PathBuf, str::FromStr};

use crate::{
    DuckCtx,
    driver::{aliases_expansion::expand_aliases, cli, typos_fixing::fix_typos},
};
use anyhow::Context;
use anyhow::anyhow;
use anyhow::bail;
use clap::ArgMatches;
use quackpack::QuackResult;
use tracing::debug;

pub(crate) fn run(ctx: &mut DuckCtx) -> QuackResult<()> {
    let external = gather_external_subcmds(ctx);
    let aliases = ctx.aliases()?;
    let cli = cli();
    let matches = cli.try_get_matches()?;
    if let Some(chdir) = matches.get_one::<PathBuf>("directory") {
        std::env::set_current_dir(chdir)
            .with_context(|| format!("couldn't change CWD to `{}`", chdir.display()))?;
    }
    let global_opts = GlobalCliOptions::from_matches(&matches)?;
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
            // TODO: If `executable` is executable, then add to commands.
        }
    }
    commands
}

fn print_parser_help(ctx: &DuckCtx) {
    let mut parser = cli();
    let help = parser.render_help();
    ctx.console().print_no_nl(help.ansi());
}

enum Color {
    Always,
    Never,
    Auto,
}

impl FromStr for Color {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "always" => Ok(Self::Always),
            "never" => Ok(Self::Never),
            "auto" => Ok(Self::Auto),
            _ => bail!(
                "`{}` is not a valid color. This should be guarded by parser",
                s
            ),
        }
    }
}

struct GlobalCliOptions {
    verbosity: u32,
    quiet: bool,
    color: Color,
}

impl GlobalCliOptions {
    fn from_matches(matches: &ArgMatches) -> QuackResult<Self> {
        let quiet = matches.get_flag("quiet");
        let verbosity = matches.get_count("verbose").into();
        let color = Color::from_str(
            matches
                .get_one::<String>("color")
                .ok_or_else(|| anyhow!("this should be guarded by default color in parser"))?,
        )?;
        Ok(Self {
            verbosity,
            quiet,
            color,
        })
    }
}
