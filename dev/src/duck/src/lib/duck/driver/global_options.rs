// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::str::FromStr;

use clap::ArgMatches;

use crate::duck::driver::cli_ext::ArgMatchesExt;
use crate::duck::util::terminal::Verbosity;
use crate::{DuckContext, QuackResult, qp_bail, qp_bail_internal};

/// Struct containing all global duck options, adjustable from cli.
#[derive(Debug)]
pub struct GlobalOptions {
    pub(in crate::duck) verbose: bool,
    pub(in crate::duck) quiet: bool,
    pub(in crate::duck) color: Color,
    pub(in crate::duck) offline: bool,
}

impl GlobalOptions {
    /// Parses the cli input to retrieve the values of the global options.
    pub fn from_matches(matches: &ArgMatches) -> Self {
        let quiet = matches.get_flag("quiet");
        let verbose = matches.get_flag("verbose");
        let offline = matches.get_flag("offline");
        let color = matches
            .get_one::<String>("color")
            .map(|color| color.parse().expect("guarded by the parser"))
            .unwrap_or(Color::Auto);
        Self {
            verbose,
            quiet,
            color,
            offline,
        }
    }

    /// Update this [`GlobalOptions`] with global flags gathered inside a subcommand.
    pub fn update_with_subcommand_matches(&mut self, matches: &ArgMatches) {
        self.quiet |= matches.safe_get_flag("quiet");
        self.verbose |= matches.safe_get_flag("verbose");
        self.offline |= matches.safe_get_flag("offline");
        if let Ok(color) = matches.safe_get_one::<String>("color").parse() {
            self.color = color;
        }
    }

    /// Updates [`DuckContext`], so that values of the global options specified by the user
    /// can be read in different parts of the program.
    pub fn update_context(&self, ctx: &mut DuckContext) -> QuackResult<()> {
        if self.verbose && self.quiet {
            qp_bail!("cannot specify both `--verbose` and `--quiet`")
        }
        if self.verbose {
            ctx.stdout_mut().set_verbosity(Verbosity::Verbose);
            ctx.stderr_mut().set_verbosity(Verbosity::Verbose);
        } else if self.quiet {
            ctx.stdout_mut().set_verbosity(Verbosity::Quiet);
            ctx.stderr_mut().set_verbosity(Verbosity::Quiet);
        }

        if matches!(self.color, Color::Never) {
            console::set_colors_enabled(false);
            console::set_colors_enabled_stderr(false);
            ctx.stdout_mut().set_color(false);
            ctx.stderr_mut().set_color(false);
        } else if matches!(self.color, Color::Always) {
            console::set_colors_enabled(true);
            console::set_colors_enabled_stderr(true);
            ctx.stdout_mut().set_color(true);
            ctx.stderr_mut().set_color(true);
        }
        ctx.set_offline(self.offline);
        Ok(())
    }
}

#[derive(Debug, Clone, Copy)]
/// Possible color output values.
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
            _ => qp_bail_internal!(
                "`{}` is not a valid color. This should be guarded by a parser",
                s,
            ),
        }
    }
}
