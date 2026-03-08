use std::str::FromStr;

use crate::{QuackResult, qp_internal};
use clap::ArgMatches;

use crate::{DuckCtx, duck::util::terminal::Verbosity};

#[derive(Debug, Clone, Copy)]
pub enum Color {
    Always,
    Never,
    Auto,
}

impl FromStr for Color {
    type Err = crate::QuackError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "always" => Ok(Self::Always),
            "never" => Ok(Self::Never),
            "auto" => Ok(Self::Auto),
            _ => Err(qp_internal!(
                "`{}` is not a valid color. This should be guarded by a parser",
                s,
            )),
        }
    }
}

#[derive(Debug)]
pub struct GlobalOptions {
    cli_options: CliOptions,
    offline: bool,
}

#[derive(Debug)]
pub struct CliOptions {
    verbose: bool,
    quiet: bool,
    color: Color,
}

impl GlobalOptions {
    pub fn from_matches(matches: &ArgMatches) -> QuackResult<Self> {
        Ok(Self {
            cli_options: CliOptions::from_matches(matches)?,
            offline: matches.get_flag("offline"),
        })
    }

    pub fn update_context(&self, ctx: &mut DuckCtx) {
        self.cli_options.update_context(ctx);
        ctx.set_offline(self.offline);
    }
}

impl CliOptions {
    pub fn from_matches(matches: &ArgMatches) -> QuackResult<Self> {
        let quiet = matches.get_flag("quiet");
        let verbose = matches.get_flag("verbose");
        let color = matches
            .get_one::<String>("color")
            .ok_or_else(|| qp_internal!("this should be guarded by a default color in the parser"))
            .and_then(|color| Color::from_str(color))?;
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
