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
mod process_builder;

use std::convert::Infallible;

pub use compilation_type::CompilationType;
use tempfile::TempDir;

use super::BuildContext;
use super::compiler_package::CompilerPackage;
use crate::quackpack::core::compile::early_graph::EarlyGraph;
use crate::quackpack::core::storage::freeze::FreezeDep;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal};

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

    /// A helper for starting a REPL session from [`DuckContext`].
    pub fn start_repl_with(ctx: &DuckContext) -> QuackResult<Infallible> {
        let this = Self::new(ctx);
        this.start_repl()
    }

    /// Start a REPL session.
    pub fn start_repl(&self) -> QuackResult<Infallible> {
        process_builder::DuckcProcessBuilder::new(self)
            .set_subcommand(process_builder::DuckcSubcommand::Repl)
            .execute_and_replace()
            .context("failed to start a REPL session")
    }

    /// Compile the `graph` with the given `compilation_type` and `bcx`.
    pub fn compile(
        &self,
        graph: &EarlyGraph,
        compilation_type: CompilationType,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<ArtifactsDir> {
        match compilation_type {
            CompilationType::OnlyRootPackage => self.compile_root_package_only(graph, bcx),
            CompilationType::StandaloneScript => self.compile_standalone_script(graph, bcx),
        }
    }

    /// Specific steps for compiling only the root package using [`process_builder`] backend.
    fn compile_root_package_only(
        &self,
        graph: &EarlyGraph,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<ArtifactsDir> {
        let this = graph.package(&graph.graph().root());
        let deps = graph
            .graph()
            .dependencies_for_package(&graph.graph().root());
        bail_if_has_deps(deps.dependencies())?;
        bail_if_has_explicit_aliases(this)?;
        let this = this.package();
        let mut builder = process_builder::DuckcProcessBuilder::new(self);
        builder
            .set_subcommand(process_builder::DuckcSubcommand::CompilePackage)
            .set_package_name(this);

        let source_dir = this.source_directory();
        if !source_dir.is_dir() {
            qp_bail!(
                "package `{}` doesn't have a `src/` directory (expected `{}` to be a directory)",
                this.as_freeze_dep(),
                source_dir.display()
            )
        }
        builder
            .set_src_dir(this)
            .set_package_artifacts_dir(this)
            .update_with_profile(&bcx.profile);
        // We need to lock a file, we can't lock a directory.
        let _lock = this.artifacts_directory().acquire_global_lock(bcx.pcx.ctx()).with_context(|| format!("failed to acquire an exclusive lock for spawning a duckc in order to compile a package `{}`", this.as_freeze_dep()))?;
        bcx.pcx
            .ctx()
            .console()
            .info_verbose(format!("Running `{}`", builder))?;
        builder.execute(|| format!("failed to compile package `{}`", this.as_freeze_dep()))?;
        Ok(ArtifactsDir::Default)
    }

    /// Specific steps for compiling a standalone script using [`process_builder`] backend.
    fn compile_standalone_script(
        &self,
        graph: &EarlyGraph,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<ArtifactsDir> {
        let venv = graph.package(&graph.graph().root());
        let deps = graph
            .graph()
            .dependencies_for_package(&graph.graph().root());
        bail_if_has_deps(deps.dependencies())?;
        bail_if_has_explicit_aliases(venv)?;
        let script_path = bcx
            .script_path
            .as_ref()
            .context_internal("Script compilation without path to it")?
            .as_path();
        let artifacts_dir = TempDir::new_in(bcx.pcx.ctx().cwd())?;
        let mut builder = process_builder::DuckcProcessBuilder::new(self);
        builder
            .set_subcommand(process_builder::DuckcSubcommand::CompileScript)
            .set_script_path(script_path)
            .set_artifacts_dir(artifacts_dir.path())
            .update_with_script_profile(&bcx.profile);
        builder.execute(|| format!("failed to compile script `{}`", script_path.display()))?;
        Ok(ArtifactsDir::TempDir(artifacts_dir))
    }
}

/// Helper for checking not yet supported features of the compiler.
fn bail_if_has_deps(dependencies: &[FreezeDep]) -> QuackResult<()> {
    if !dependencies.is_empty() {
        qp_bail_internal!("external dependencies are not (yet) supported by duckc")
    }
    Ok(())
}

/// Helper for checking not yet supported features of the compiler.
fn bail_if_has_explicit_aliases(package: &CompilerPackage) -> QuackResult<()> {
    let manifest = package.package().manifest();
    if manifest
        .dependencies()
        .all_dependencies()
        .iter()
        .any(|dep| dep.is_aliased())
    {
        let desc = package.package().as_freeze_dep();
        qp_bail_internal!("package `{desc}` has aliased dependencies, which is not yet supported")
    }
    Ok(())
}
