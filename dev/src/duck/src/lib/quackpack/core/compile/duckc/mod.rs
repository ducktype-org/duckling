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
mod process_builder;

pub use compilation_type::CompilationType;

use super::BuildContext;
use super::compiler_package::CompilerPackage;
use crate::quackpack::core::compile::compiler_dag::CompilerDag;
use crate::quackpack::core::storage::freeze::FreezeDep;
use crate::util_common::path_ops_ext::{PathOpsExt, ShouldBlock};
use crate::{DuckCtx, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal};

#[derive(Debug)]
/// Data holder of all required in order to execute the compiler.
pub struct Duckc {
    program_name: StrId,
}

impl Duckc {
    /// Create new [`Duckc`] from the [`DuckCtx`].
    pub fn new(ctx: &DuckCtx) -> Self {
        let _ = ctx;
        Self {
            program_name: "duckc".into(),
        }
    }

    /// Compile the `graph` with the given `compilation_type` and `bcx`.
    pub fn compile(
        &self,
        graph: &CompilerDag,
        compilation_type: CompilationType,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<()> {
        match compilation_type {
            CompilationType::OnlyRootPackage => self.compile_root_package_only(graph, bcx),
        }
    }

    /// Specific steps for compiling only the root package using [`process_builder`] backend.
    fn compile_root_package_only(
        &self,
        graph: &CompilerDag,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<()> {
        let this = graph.package(&graph.dag().root())?;
        let deps = graph.dag().dependencies_for_package(&graph.dag().root())?;
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
        let _lock = this.artifacts_directory().join(".duck_lock").lock(ShouldBlock::Yes).with_context(|| format!("failed to acquire an exclusive lock for spawning a duckc in order to compile a package `{}`", this.as_freeze_dep()))?;
        bcx.package
            .ctx()
            .console()
            .info_verbose(format!("Running `{}`", builder));
        builder.execute(this.as_freeze_dep())?;
        Ok(())
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
