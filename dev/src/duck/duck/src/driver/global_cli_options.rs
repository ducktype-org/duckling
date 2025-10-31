use std::str::FromStr;

use anyhow::anyhow;
use clap::ArgMatches;
use quackpack::QuackResult;

use crate::InternalError;
use crate::{DuckCtx, terminal::Verbosity};

#[derive(Debug, Clone, Copy)]
pub enum Color {
    Always,
    Never,
    Auto,
}

impl FromStr for Color {
    type Err = crate::InternalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "always" => Ok(Self::Always),
            "never" => Ok(Self::Never),
            "auto" => Ok(Self::Auto),
            _ => Err(anyhow!(
                "`{}` is not a valid color. This should be guarded by a parser",
                s
            )
            .into()),
        }
    }
}

#[derive(Debug)]
pub struct GlobalCliOptions {
    verbose: bool,
    quiet: bool,
    color: Color,
}

impl GlobalCliOptions {
    pub fn from_matches(matches: &ArgMatches) -> QuackResult<Self> {
        let quiet = matches.get_flag("quiet");
        let verbose = matches.get_flag("verbose");
        let color = Color::from_str(matches.get_one::<String>("color").ok_or_else(|| {
            InternalError::from(anyhow!(
                "this should be guarded by a default color in parser"
            ))
        })?)?;
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
            console::set_colors_enabled(false);
            console::set_colors_enabled_stderr(false);
            ctx.console_mut().set_color(false);
            ctx.error_console_mut().set_color(false);
        } else if matches!(self.color, Color::Always) {
            console::set_colors_enabled(true);
            console::set_colors_enabled_stderr(true);
            ctx.console_mut().set_color(true);
            ctx.error_console_mut().set_color(true);
        }
    }
}
