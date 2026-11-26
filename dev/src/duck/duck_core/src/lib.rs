// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! The base shared by `duck`, QuackPack and the other tools of the duck ecosystem.
//!
//! It holds what gives every tool the same UX and behaviour, and nothing that is specific to
//! package management://! - [`cli`]: shared [`clap`] helpers and styles,
//! - [`duck_context`]: [`DuckContext`], with the terminal, the environment, the user configuration and
//!   the duck home,
//! - [`package_root`]: locating a package root, without parsing its manifest
//! - [`pipeline`]: the common pipeline of a duck tool (logger setup, error printing, exit code),
//! - [`util`]: universal utilities for example filesystem helpers, interning ([`StrId`]), hashing, YAML config, wording,
//! - [`schemas`]: the schema of [`DuckCfg`](duck_context::DuckCfg).

pub mod cli;
pub mod duck_context;
pub mod error;
pub mod package_root;
pub mod pipeline;
pub mod schemas;
pub mod util;

pub use duck_context::DuckContext;
pub use error::{QuackError, QuackResultContext};
pub use util::interning::StrId;

/// A common [`Result`] type used widely throughout the project.
pub type QuackResult<T> = Result<T, QuackError>;
