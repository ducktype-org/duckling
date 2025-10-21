use std::str::FromStr;

use anyhow::{anyhow, bail};
use clap::ArgMatches;
use quackpack::QuackResult;

use crate::{DuckCtx, terminal::Verbosity};

#[derive(Debug)]
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
            // TODO: Introduce our own `Internal` error type.
            //       Then also update `main::print_error_and_exit`, to check for internal errors,
            //       in error chain.
            _ => bail!(
                "`{}` is not a valid color. This should be guarded by parser",
                s
            ),
        }
    }
}

pub struct GlobalCliOptions {
    verbose: bool,
    quiet: bool,
    color: Color,
}

impl GlobalCliOptions {
    pub fn from_matches(matches: &ArgMatches) -> QuackResult<Self> {
        let quiet = matches.get_flag("quiet");
        let verbose = matches.get_flag("verbose");
        let color = Color::from_str(
            matches
                .get_one::<String>("color")
                .ok_or_else(|| anyhow!("this should be guarded by default color in parser"))?,
        )?;
        Ok(Self {
            verbose,
            quiet,
            color,
        })
    }

    pub fn update_context(&self, ctx: &mut DuckCtx) {
        if self.verbose {
            ctx.console_mut().set_verbosity(Verbosity::Verbose);
            ctx.error_console_mut().set_verbosity(Verbosity::Verbose);
        } else if self.quiet {
            ctx.console_mut().set_verbosity(Verbosity::Quiet);
            ctx.error_console_mut().set_verbosity(Verbosity::Quiet);
        }

        if matches!(self.color, Color::Never) {
            // TODO: We might also want to this (somehow) as a wrapper to consoles.
            //       https://docs.rs/console/latest/console/struct.WithoutAnsi.html
            console::set_colors_enabled(false);
            console::set_colors_enabled_stderr(false);
        } else if matches!(self.color, Color::Always) {
            console::set_colors_enabled(true);
            console::set_colors_enabled_stderr(true);
        }
    }
}
