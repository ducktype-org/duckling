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
}
