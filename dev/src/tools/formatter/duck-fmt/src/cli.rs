// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Command line interface of `duck-fmt`.

use std::ffi::OsString;

use clap::{Arg, Command, value_parser};
use duck_core::cli::flag;
use duck_core::cli::styles::get_styles;

/// Returns command line interface of `duck-fmt` as a [`Command`].
pub(super) fn cli() -> Command {
    Command::new("duck-fmt")
        .arg(
            flag("check", "Do not rewrite the files; report each unformatted file on stderr and exit non-zero if any of them is not already formatted")
            .short('k')
        )
        .arg(
            flag("print", "Do not rewrite the files; print each formatted file on stdout")
            .short('p')
            .conflicts_with("check")
        )
        .arg(
            Arg::new("args")
                .help("Additional arguments passed to `duckfmt`")
                .trailing_var_arg(true)
                .num_args(0..)
                .value_parser(value_parser!(OsString))
        )
        .styles(get_styles())
        .about("Formats all Duckling files inside the current package using `duckfmt`")
}
