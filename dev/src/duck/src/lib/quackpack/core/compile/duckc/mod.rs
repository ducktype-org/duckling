// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Main interaction with the compiler.
//!
//! It ~~supports~~ will support different types of interactions and compilation types,
//! but right now it supports only compiling the root package and fork&exec communication.
//!
//! Notable objects are:
//! - [`Duckc`][]: object with all required informations for communicating with the compiler,
//! - [`compilation_type`][]: supported types of compilations,
//! - [`process_builder`][]: [`Command`](std::process::Command) backed backend for fork&exec
//!   communication with the compiler.

mod compilation_type;
pub mod multipackage_schema;
pub mod process_builder;
mod version;

use std::convert::Infallible;
use std::path::Path;

pub use compilation_type::CompilationType;
use tempfile::TempDir;

use crate::{DuckContext, QuackResult, QuackResultContext, StrId};

#[derive(Debug)]
/// Data holder of all required in order to execute the compiler.
pub struct Duckc {
    program_name: StrId,
}

#[derive(Debug)]
/// Represents where the compilation artifacts are stored.
pub enum ArtifactsDir {
    Default,
    TempDir(TempDir),
}

impl Duckc {
    /// Create new [`Duckc`] from the [`DuckContext`].
    pub fn new(ctx: &DuckContext) -> Self {
        let _ = ctx;
        Self {
            program_name: "duckc".into(),
        }
    }

    /// Get [`process_builder::DuckcProcessBuilder`] inferred from this [`Duckc`].
    pub fn process_builder(&self) -> process_builder::DuckcProcessBuilder {
        process_builder::DuckcProcessBuilder::new(self)
    }

    /// Start a REPL session, optionally with a `--packages` manifest and extra `duckc repl`
    /// arguments.
    pub fn start_repl(
        &self,
        packages_manifest: Option<&Path>,
        extra_args: &[String],
    ) -> QuackResult<Infallible> {
        let mut builder = self.process_builder();
        builder.set_subcommand(process_builder::DuckcSubcommand::Repl);
        if let Some(manifest) = packages_manifest {
            builder.set_repl_packages(manifest);
        }
        builder
            .add_args(extra_args)
            .execute_and_replace()
            .context("failed to start a REPL session")
    }
}
