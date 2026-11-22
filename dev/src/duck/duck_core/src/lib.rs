// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! The base shared by `duck`, QuackPack and the other tools of the duck ecosystem.
//!
//! It holds what gives every tool the same UX and behaviour, and nothing that is specific to
//! package management:
//! - [`util`]: universal utilities, usable by any tool without pulling in anything else — errors
//!   ([`QuackError`]), filesystem helpers, interning ([`StrId`]), hashing, YAML config, wording,
//! - [`context`]: [`DuckContext`], with the terminal, the environment, the user configuration and
//!   the duck home,
//! - [`cli`]: shared `clap` helpers and styles,
//! - [`entry`]: the common entry point of a duck tool (logger setup, error printing, exit code),
//! - [`package_root`]: locating a package root, without parsing its manifest.
pub mod cli;
pub mod context;
pub mod entry;
pub mod package_root;
pub mod schemas;
pub mod util;

pub use context::duck_context::DuckContext;
pub use util::error::{QuackError, QuackResultContext};
pub use util::interning::str_id::*;

/// A common [`Result`] type used widely throughout the project.
pub type QuackResult<T> = Result<T, QuackError>;
