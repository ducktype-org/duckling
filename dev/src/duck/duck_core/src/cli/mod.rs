// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Shared command-line helpers, so every duck tool parses and looks the same.

pub mod styles;

use clap::{Arg, ArgAction, Command};

/// Create a new boolean argument.
pub fn flag(name: &'static str, help: &'static str) -> Arg {
    Arg::new(name)
        .help(help)
        .long(name)
        .action(ArgAction::SetTrue)
}

/// Create a new optional flag.
pub fn optional(name: &'static str, help: &'static str) -> Arg {
    Arg::new(name).help(help).long(name).action(ArgAction::Set)
}

/// Create an argument which takes multiple values.
pub fn multi(name: &'static str, help: &'static str) -> Arg {
    Arg::new(name)
        .help(help)
        .long(name)
        .action(ArgAction::Append)
}

/// Create a new subcommand.
pub fn subcommand(name: &'static str) -> Command {
    Command::new(name)
}
