//! [`UnitRunner`] takes a [`UnitGraph`] and [`UnitTaskGenerator`] and compilation using it.

use std::fmt;
use std::io::Write;
use std::path::PathBuf;
use std::process::ExitStatus;

use tracing::{debug, error, info, instrument, trace};

use super::BuildContext;
use super::artifacts_layout::shared::SharedArtifactsLayout;
use super::artifacts_layout::standard::StandardArtifactsLayout;
use super::artifacts_layout::{ArtifactsLayout, DependencyLayout, ProfileLayout};
use super::duckc::process_builder::{self, DuckcSubcommand};
use super::duckc::{Duckc, multipackage_schema};
use super::profiles::Profile;
use super::unit::Unit;
use super::unit::graph::UnitGraph;
use super::unit_task_generator::UnitTaskGenerator;
use super::unit_task_generator::default::DefaultTaskGenerator;
use super::unit_task_generator::dvm::DvmTaskGenerator;
use crate::quackpack::core::compile::unit::BuildKind;
use crate::util::file_locks::LockedFile;
use crate::{QuackResult, QuackResultContext, qp_bail};

pub mod external_libs;
pub mod outputs;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// A runner of [`Unit`]s.
pub struct UnitRunner<'duck, 'ctx> {
    graph: UnitGraph,
    task_generator: Box<dyn UnitTaskGenerator>,
    bcx: &'ctx BuildContext<'duck, 'ctx>,
}

impl<'duck, 'ctx> UnitRunner<'duck, 'ctx> {
    /// Create a new [`UnitRunner`].
    pub fn new(graph: UnitGraph, bcx: &'ctx BuildContext<'duck, 'ctx>) -> Self {
        let task_generator = bcx.task_generator();
        Self {
            graph,
            task_generator,
            bcx,
        }
    }

    #[instrument(skip_all)]
    /// Drive the compilation with [`UnitRunner`].
    pub fn compile(self) -> QuackResult<CompilationOutput> {
        self.task_generator.pre_compilation(&self.graph, self.bcx)?;
        let artifacts_layout = self.bcx.artifacts_layout(&self.graph);
        let profile_layout = artifacts_layout.for_profile(self.bcx.profile);
        self.compile_all_needed_units(&*profile_layout)?;
        outputs::get_compiler_output(&self.graph, &*profile_layout)
    }

    #[instrument(skip_all)]
    /// Compile all needed [`Unit`]s, as determined by [`should_compile`].
    ///
    /// [`should_compile`]: UnitTaskGenerator::should_compile
    fn compile_all_needed_units(&self, layout: &dyn ProfileLayout) -> QuackResult<()> {
        for unit in self.graph.compilation_order() {
            if !self
                .task_generator
                .should_compile(unit, &self.graph, self.bcx)
            {
                continue;
            }
            match unit.build_kind() {
                BuildKind::Compile => self.compile_unit(unit, layout)?,
            }
        }
        Ok(())
    }

    #[instrument(skip_all, fields(id = %unit.unit_id(), name = %unit.package().name(), version = %unit.package().version(), identity = %unit.identity()))]
    /// Compile a single [`Unit`].
    /// May panic, if this [`Unit`] shouldn't be compiled ([`should_compile`] returned `false`).
    ///
    /// [`should_compile`]: UnitTaskGenerator::should_compile
    fn compile_unit(&self, unit: &Unit, layout: &dyn ProfileLayout) -> QuackResult<()> {
        info!("starting compilation of a unit");
        let tasks = self
            .task_generator
            .create_tasks(unit, &self.graph, layout, self.bcx)?;
        self.compile_unit_with_tasks(unit, layout, tasks)
    }

    /// Compile a single [`Unit`] with its finished tasks.
    #[instrument(skip_all)]
    fn compile_unit_with_tasks(
        &self,
        unit: &Unit,
        layout: &dyn ProfileLayout,
        tasks: Vec<multipackage_schema::Task>,
    ) -> QuackResult<()> {
        trace!(?tasks);
        let packages = outputs::collect_packages(unit, &self.graph)?;
        let schema = multipackage_schema::MultiPackage { packages, tasks };
        trace!(?schema);
        self.compile_unit_with_schema(unit, layout, schema)
    }

    /// Compile a single [`Unit`] with its finished schema.
    #[instrument(skip_all)]
    fn compile_unit_with_schema(
        &self,
        unit: &Unit,
        layout: &dyn ProfileLayout,
        schema: multipackage_schema::MultiPackage,
    ) -> QuackResult<()> {
        let name = unit.package().name();
        let status = (|| {
            let unit_layout = layout.for_dependency(unit, &self.graph)?;
            let builder =
                self.finished_builder_for_layout_and_profile(&*unit_layout, self.bcx.profile);
            let _lock = unit_layout.acquire_lock(self.bcx.pcx.ctx())?;
            let locked_manifest_file = unit_layout
                .dependency_json(self.bcx.pcx.ctx())
                .context("failed to open `deps.json`")?;
            write_schema(schema, &locked_manifest_file)?;
            // Ensure we flush, by dropping the inner `File`.
            drop(locked_manifest_file);
            self.compile_and_print(builder, name)
        })()
        .with_context(|| format!("failed to compile `{name}`"))?;
        if !status.success() {
            error!(%status, "compilation failed, status is not success");
            qp_bail!("failed to compile `{name}`")
        }
        info!("successfully compiled a unit");
        Ok(())
    }

    /// Create a basic and reusable [`process_builder::DuckcProcessBuilder`].
    fn finished_builder_for_layout_and_profile(
        &self,
        layout: &dyn DependencyLayout,
        profile: Profile,
    ) -> process_builder::DuckcProcessBuilder {
        let mut builder = Duckc::new(self.bcx.pcx.ctx()).process_builder();
        builder
            .set_subcommand(DuckcSubcommand::CompilePackages)
            .set_manifest_path(&layout.dependency_json_path())
            .set_artifacts_dir(&layout.compiler_artifacts())
            .set_c_std(profile.c_std)
            .set_opt_level(profile.opt_level)
            .set_incremental(profile.incremental)
            .set_workers_count(self.bcx.jobs);
        builder
    }

    /// Execute a builder, and print messages.
    #[instrument(skip_all)]
    fn compile_and_print(
        &self,
        mut builder: process_builder::DuckcProcessBuilder,
        name: impl fmt::Display,
    ) -> QuackResult<ExitStatus> {
        self.bcx.pcx.ctx().info(format!("compiling `{name}`..."))?;
        self.bcx
            .pcx
            .ctx()
            .info_verbose(format!("Running `{}`", builder))?;
        builder.execute()
    }
}

impl BuildContext<'_, '_> {
    /// Get an appropriate [`UnitTaskGenerator`].
    pub fn task_generator(&self) -> Box<dyn UnitTaskGenerator> {
        if self.profile.dvm_bytecode {
            debug!("returning DvmTaskGenerator");
            return Box::new(DvmTaskGenerator);
        }
        debug!("returning DefaultTaskGenerator");
        Box::new(DefaultTaskGenerator)
    }

    /// Get an appropriate [`ArtifactsLayout`] implementation.
    pub fn artifacts_layout(&self, graph: &UnitGraph) -> Box<dyn ArtifactsLayout> {
        let root_package = graph.root_unit().package().get_package();
        let root_package_artifacts = root_package.artifacts_directory().to_path_buf();
        debug!(uses_shared_artifacts = %self.shared);
        if self.shared {
            Box::new(SharedArtifactsLayout::new(root_package_artifacts))
        } else {
            Box::new(StandardArtifactsLayout::new(root_package_artifacts))
        }
    }
}

#[derive(Debug)]
/// Output of [`compile`].
///
/// [`compile`]: UnitRunner::compile
pub struct CompilationOutput {
    /// Root [`Unit`] and path to its output.
    pub root: (Unit, PathBuf),
}

/// Write a manifest into a file.
#[instrument(skip_all)]
fn write_schema(
    schema: multipackage_schema::MultiPackage,
    locked_manifest_file: &LockedFile,
) -> QuackResult<()> {
    let manifest_json = serde_json::to_string_pretty(&schema).with_context_internal(|| {
        format!("failed to convert manifest into a JSON string: {schema:#?}")
    })?;
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
