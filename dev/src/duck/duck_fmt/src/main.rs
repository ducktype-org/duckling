// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! `duck-fmt` --- package-aware formatter for Duckling, working as an external subcommand for `duck`.
//! It is a thin wrapper on lower-level `duckfmt`, enriching it with package-related knowledge.
//! Submodules:
//! - [`cli`] - command line interface of `duck-fmt`,
//! - [`duckfmt`] - builder of the `duckfmt` process,
//! - [`fmt_config`] - locating `duckfmt` config inside a package,
//! - [`formatting`] - main logic for formatting a package.

use duck_core::*;

use crate::formatting::{FormatOptions, PackageFormatter};

mod cli;
mod duckfmt;
mod fmt_config;
mod formatting;

/// Main entry point to the `duck-fmt` binary.
/// Reuses [`duck::duck::main_runner`] abstraction.
fn main() {
    pipeline::main(run);
}

/// Main pipeline of `duck-fmt`.
fn run(ctx: &mut DuckContext) -> QuackResult<()> {
    let cli = cli::cli();
    let matches = cli.try_get_matches()?;
    let options = FormatOptions::from_matches(&matches);
    let formatter = PackageFormatter::new(ctx, options)?;
    formatter.format()
}
