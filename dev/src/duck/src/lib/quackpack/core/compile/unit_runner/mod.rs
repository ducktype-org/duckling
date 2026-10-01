//! [`UnitRunner`] takes a [`UnitGraph`] and a [`UnitTaskGenerator`] and drives the compilation
//! process using them.

use std::cell::RefCell;
use std::collections::HashMap;
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
use super::unit::graph::UnitGraph;
use super::unit::{Unit, UnitId};
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
    unit_statuses: RefCell<HashMap<UnitId, UnitStatus>>,
    bcx: &'ctx BuildContext<'duck, 'ctx>,
}

impl<'duck, 'ctx> UnitRunner<'duck, 'ctx> {
    /// Create a new [`UnitRunner`].
    pub fn new(graph: UnitGraph, bcx: &'ctx BuildContext<'duck, 'ctx>) -> Self {
        let task_generator = bcx.task_generator();
        let unit_statuses = RefCell::new(build_initial_unit_statuses(&graph));
        Self {
            graph,
            task_generator,
            unit_statuses,
            bcx,
        }
    }

    #[instrument(skip_all)]
    /// Drive the compilation with [`UnitRunner`].
    pub fn run(self) -> QuackResult<CompilationOutput> {
        self.task_generator.pre_compilation(&self.graph, self.bcx)?;
        let artifacts_layout = self.bcx.artifacts_layout(&self.graph);
        let profile_layout = artifacts_layout.for_profile(self.bcx.profile);
        self.run_all_needed_units(&*profile_layout)?;
        outputs::get_compiler_output(&self.graph, &*profile_layout, self.bcx)
    }

    #[instrument(skip_all)]
    /// Compile all needed [`Unit`]s, as determined by [`should_run`].
    ///
    /// [`should_run`]: UnitTaskGenerator::should_run
    fn run_all_needed_units(&self, layout: &dyn ProfileLayout) -> QuackResult<()> {
        // @TODO: #3636 We should have a dedicated struct for determining compilation order.
        for unit in self.graph.compilation_order() {
            if !self.task_generator.should_run(unit, &self.graph, self.bcx) {
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
    /// May panic, if this [`Unit`] shouldn't be compiled ([`should_run`] returned `false`).
    ///
    /// [`should_run`]: UnitTaskGenerator::should_run
    fn compile_unit(&self, unit: &Unit, layout: &dyn ProfileLayout) -> QuackResult<()> {
        info!("starting compilation of a unit");
        self.set_unit_status(unit, UnitStatus::InProgress);
        let tasks = self
            .task_generator
            .create_tasks(unit, &self.graph, layout, self.bcx)?;
        self.compile_unit_with_tasks(unit, layout, tasks)?;
        self.set_unit_status(unit, UnitStatus::Finished);
        Ok(())
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

    /// Set a [`UnitStatus`] for the given [`Unit`].
    fn set_unit_status(&self, unit: &Unit, status: UnitStatus) {
        let id = unit.unit_id();
        debug_assert!(
            self.unit_statuses.borrow().contains_key(&id),
            "missing status for unit {unit:?}, but we set an initial status for every unit"
        );
        self.unit_statuses.borrow_mut().insert(id, status);
    }
}

impl BuildContext<'_, '_> {
    /// Get an appropriate [`UnitTaskGenerator`].
    pub fn task_generator(&self) -> Box<dyn UnitTaskGenerator> {
        if self.targets_dvm() {
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

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// Status of [`Unit`] during [`run`] pass.
///
/// [`run`]: UnitRunner::run
enum UnitStatus {
    /// This [`Unit`] hasn't been started.
    NotStarted,
    /// We're in progress of running this [`Unit`].
    InProgress,
    /// This [`Unit`] has finished compiling.
    Finished,
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// Which compilation backend we target.
pub enum CompilationTarget {
    /// We have compiled to LLVM.
    LLVM,
    /// We have compiled to DVM.
    DVM,
}

impl CompilationTarget {
    /// Returns `true` if the compilation target is [`LLVM`].
    ///
    /// [`LLVM`]: CompilationTarget::LLVM
    #[must_use]
    pub fn is_llvm(self) -> bool {
        matches!(self, Self::LLVM)
    }

    /// Returns `true` if the compilation target is [`DVM`].
    ///
    /// [`DVM`]: CompilationTarget::DVM
    #[must_use]
    pub fn is_dvm(self) -> bool {
        matches!(self, Self::DVM)
    }
}

#[derive(Debug)]
/// Output of [`run`].
///
/// [`run`]: UnitRunner::run
pub struct CompilationOutput {
    /// Root [`Unit`] and path to its output.
    pub root: (Unit, PathBuf),
    pub target: CompilationTarget,
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

/// Create an initial map with statuses of all known [`Unit`]s.
///
/// By default all are [`NotStarted`]
///
/// [`NotStarted`]: UnitStatus::NotStarted
fn build_initial_unit_statuses(graph: &UnitGraph) -> HashMap<UnitId, UnitStatus> {
    graph
        .units_sorted_by_id()
        .iter()
        .map(|unit| (unit.unit_id(), UnitStatus::NotStarted))
        .collect()
}
