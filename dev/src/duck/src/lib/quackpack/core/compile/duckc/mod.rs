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

use std::convert::Infallible;

pub use compilation_type::CompilationType;
use tempfile::TempDir;
use tracing::error;

use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail};

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

    /// A helper for starting a REPL session from [`DuckContext`].
    pub fn start_repl_with(ctx: &DuckContext) -> QuackResult<Infallible> {
        let this = Self::new(ctx);
        this.start_repl()
    }

    /// Start a REPL session.
    pub fn start_repl(&self) -> QuackResult<Infallible> {
        self.process_builder()
            .set_subcommand(process_builder::DuckcSubcommand::Repl)
            .execute_and_replace()
            .context("failed to start a REPL session")
    }

    /// Get the version of `duckc`.
    pub fn get_version(ctx: &DuckContext) -> QuackResult<String> {
        let this = Self::new(ctx);
        let output = this
            .process_builder()
            .ask_for_version()
            .execute_and_get_output()?;
        let status = output.status;
        if !status.success() {
            error!(%status, "invoking duckc failed, status is not success");
            // Maybe print stderr here?
            qp_bail!("failed to run `duckc --version`")
        }
        String::from_utf8(output.stdout).with_context(|| "`duckc --version` returned invalid utf8")
    }
}
