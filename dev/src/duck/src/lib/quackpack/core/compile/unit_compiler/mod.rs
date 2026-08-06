//! [`UnitCompiler`] takes a [`UnitGraph`] and compiles it, according to its strategy.

use std::fmt::{self, Debug};
use std::io::Write;
use std::path::PathBuf;
use std::process::ExitStatus;

pub mod default_unit_compiler;
pub mod dvm_unit_compiler;
pub mod outputs;

#[cfg(test)]
mod tests;

use tracing::{debug, error, info, instrument, trace};

use self::default_unit_compiler::DefaultUnitCompiler;
use self::dvm_unit_compiler::DvmUnitCompiler;
use super::BuildContext;
use super::artifacts_layout::shared::SharedArtifactsLayout;
use super::artifacts_layout::standard::StandardArtifactsLayout;
use super::artifacts_layout::{ArtifactsLayout, DependencyLayout, ProfileLayout};
use super::duckc::process_builder::DuckcSubcommand;
use super::duckc::{Duckc, multipackage_schema, process_builder};
use super::profiles::Profile;
use super::unit::Unit;
use super::unit::graph::UnitGraph;
use crate::util::file_locks::LockedFile;
use crate::{QuackResult, QuackResultContext, qp_bail};

/// A generic duckc driver.
pub trait UnitCompiler: Debug {
    /// Callback invoked at the very start of [`compile`].
    ///
    /// Right now, this function performs [`assert`]sions about the mode (f.e. that
    /// [`DvmUnitCompiler`] actually tries to compile DVM packages).
    fn pre_compilation(&self, graph: &UnitGraph, bcx: &BuildContext<'_, '_>);

    /// Create tasks which will be used for compiling the given [`Unit`].
    fn create_tasks(
        &self,
        unit: &Unit,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<Vec<multipackage_schema::Task>>;

    /// Get the list of [`Unit`]s to compile.
    ///
    /// [`Unit`]s will be compiled in order determined by the returned vector, starting from the
    /// index 0.
    fn units_to_compile<'a>(
        &self,
        graph: &'a UnitGraph,
        bcx: &BuildContext<'_, '_>,
    ) -> Vec<&'a Unit>;
}

#[derive(Debug)]
/// Output of [`compile`].
pub struct UnitCompilerOutput {
    /// Root [`Unit`] and path to its output.
    pub root: (Unit, PathBuf),
}

impl BuildContext<'_, '_> {
    /// Get an appropriate [`UnitCompiler`].
    pub fn unit_compiler(&self) -> Box<dyn UnitCompiler> {
        if self.profile.dvm_bytecode {
            debug!("returning DvmUnitCompiler");
            return Box::new(DvmUnitCompiler);
        }
        debug!("returning DefaultUnitCompiler");
        Box::new(DefaultUnitCompiler)
    }

    /// Get an appropriate [`ArtifactsLayout`] implementation.
    pub fn artifacts_layout(&self, graph: &UnitGraph) -> Box<dyn ArtifactsLayout> {
        let root_package = graph.root_unit().root_package().package().get_package();
        let root_package_artifacts = root_package.artifacts_directory().to_path_buf();
        debug!(uses_shared_artifacts = %self.shared);
        if self.shared {
            Box::new(SharedArtifactsLayout::new(root_package_artifacts))
        } else {
            Box::new(StandardArtifactsLayout::new(root_package_artifacts))
        }
    }
}

#[instrument(skip_all)]
/// Compile the given [`UnitGraph`], driven by the [`UnitCompiler`].
pub fn compile(
    compiler: &dyn UnitCompiler,
    graph: UnitGraph,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<UnitCompilerOutput> {
    compiler.pre_compilation(&graph, bcx);
    let artifacts_layout = bcx.artifacts_layout(&graph);
    let profile_layout = artifacts_layout.for_profile(bcx.profile);
    compile_all_needed_units(compiler, &graph, &*profile_layout, bcx)?;
    outputs::get_compiler_output(&graph, &*profile_layout)
}

#[instrument(skip_all)]
/// Compile all [`Unit`]s from the [`units_to_compile`](UnitCompiler::units_to_compile) list.
fn compile_all_needed_units(
    compiler: &dyn UnitCompiler,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<()> {
    for unit in compiler.units_to_compile(graph, bcx) {
        compile_unit(compiler, unit, graph, layout, bcx)?;
    }
    Ok(())
}

#[instrument(skip_all, fields(id = %unit.unit_id(), name = %unit.root_package().package().name(), version = %unit.root_package().package().version(), identity = %unit.identity()))]
/// Compile a single [`Unit`].
/// May panic, if this [`Unit`] is not in the [`units_to_compile`](Self::units_to_compile) list.
fn compile_unit(
    compiler: &dyn UnitCompiler,
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<()> {
    info!("starting compilation of a unit");
    let tasks = compiler.create_tasks(unit, graph, layout, bcx)?;
    compile_unit_with_tasks(unit, graph, layout, bcx, tasks)
}

/// Compile a single [`Unit`] with its finished tasks.
#[instrument(skip_all)]
fn compile_unit_with_tasks(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
    tasks: Vec<multipackage_schema::Task>,
) -> QuackResult<()> {
    trace!(?tasks);
    let packages = outputs::collect_packages(unit, graph)?;
    let schema = multipackage_schema::MultiPackage { packages, tasks };
    trace!(?schema);
    compile_unit_with_schema(unit, graph, layout, bcx, schema)
}

/// Compile a single [`Unit`] with its finished schema.
#[instrument(skip_all)]
fn compile_unit_with_schema(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
    schema: multipackage_schema::MultiPackage,
) -> QuackResult<()> {
    let name = unit.root_package().package().name();
    let status = (|| {
        let unit_layout = layout.for_dependency(unit, graph)?;
        let builder = finished_builder_for_layout_and_profile(bcx, &*unit_layout, &bcx.profile);
        let _lock = unit_layout.acquire_lock(bcx.pcx.ctx())?;
        let locked_manifest_file = unit_layout
            .dependency_json(bcx.pcx.ctx())
            .context("failed to open `deps.json`")?;
        write_schema(schema, &locked_manifest_file)?;
        // Ensure we flush, by dropping the inner `File`.
        drop(locked_manifest_file);
        compile_and_print(bcx, builder, name)
    })()
    .with_context(|| format!("failed to compile `{name}`"))?;
    if !status.success() {
        error!(%status, "compilation failed, status is not success");
        qp_bail!("failed to compile `{name}`")
    }
    info!("successfully compiled a unit");
    Ok(())
}

/// Write a manifest into a file.
#[instrument(skip_all)]
fn write_schema(
    schema: multipackage_schema::MultiPackage,
    locked_manifest_file: &LockedFile,
) -> QuackResult<()> {
    let manifest_json = serde_json::to_string_pretty(&schema)
        .context_internal("failed to convert manifest into a JSON string")?;
    locked_manifest_file.file().set_len(0).with_context(|| {
        format!(
            "failed to truncate `{}`",
            locked_manifest_file.path().display()
        )
    })?;
    locked_manifest_file
        .file()
        .write_all(manifest_json.as_bytes())
        .with_context(|| {
            format!(
                "failed to write manifest to `{}`",
                locked_manifest_file.path().display()
            )
        })?;
    Ok(())
}

/// Create a basic and reusable [`process_builder::DuckcProcessBuilder`].
fn finished_builder_for_layout_and_profile(
    bcx: &BuildContext<'_, '_>,
    layout: &dyn DependencyLayout,
    profile: &Profile,
) -> process_builder::DuckcProcessBuilder {
    let mut builder = Duckc::new(bcx.pcx.ctx()).process_builder();
    builder
        .set_subcommand(DuckcSubcommand::CompilePackages)
        .set_manifest_path(&layout.dependency_json_path())
        .set_artifacts_dir(&layout.compiler_artifacts())
        .set_c_std(profile.c_std)
        .set_opt_level(profile.opt_level)
        .set_incremental(profile.incremental)
        .set_workers_count(bcx.jobs);
    builder
}

/// Execute a builder, and print messages.
#[instrument(skip_all)]
fn compile_and_print(
    bcx: &BuildContext<'_, '_>,
    mut builder: process_builder::DuckcProcessBuilder,
    name: impl fmt::Display,
) -> QuackResult<ExitStatus> {
    bcx.pcx
        .ctx()
        .console()
        .info(format!("compiling `{name}`..."))?;
    bcx.pcx
        .ctx()
        .console()
        .info_verbose(format!("Running `{}`", builder))?;
    builder.execute()
}
