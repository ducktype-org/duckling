// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use core::fmt;
use std::cell::RefCell;
use std::collections::HashMap;
use std::io::Write;
use std::path::PathBuf;
use std::process::ExitStatus;

use tracing::{debug, error, info, instrument, trace};

use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::shared::SharedArtifactsLayout;
use crate::quackpack::core::compile::artifacts_layout::standard::StandardArtifactsLayout;
use crate::quackpack::core::compile::artifacts_layout::{
    ArtifactsLayout, DependencyLayout, ProfileLayout,
};
use crate::quackpack::core::compile::duckc::process_builder::{self, DuckcSubcommand};
use crate::quackpack::core::compile::duckc::{Duckc, multipackage_schema};
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraph};
use crate::quackpack::core::compile::unit::{Unit, UnitType};
use crate::quackpack::core::compile::unit_task_generator::UnitTaskGenerator;
use crate::quackpack::core::compile::unit_task_generator::default::DefaultTaskGenerator;
use crate::quackpack::core::compile::unit_task_generator::dvm::DvmTaskGenerator;
use crate::util::file_locks::LockedFile;
use crate::{QuackResult, QuackResultContext, qp_bail};

pub mod external_libs;
pub mod outputs;

#[derive(Debug)]
/// A runner of [`Unit`]s.
pub struct UnitRunner<'duck, 'ctx> {
    graph: UnitGraph,
    task_generator: Box<dyn UnitTaskGenerator>,
    unit_statuses: RefCell<HashMap<GraphNodeId, UnitStatus>>,
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
        for node in self.graph.compilation_order() {
            let node_id = node.id();
            let Some(unit) = node.as_unit() else {
                continue;
            };
            if !self.task_generator.should_run(unit, &self.graph, self.bcx) {
                continue;
            }
            match unit.unit_type() {
                UnitType::Binary | UnitType::Library | UnitType::Dependency => {
                    self.compile_unit(unit, node_id, layout)?;
                }
            }
        }
        Ok(())
    }

    #[instrument(skip_all, fields(id = %id_in_graph, name = %unit.package().name(), version = %unit.package().version(), identity = %unit.identity()))]
    /// Compile a single [`Unit`].
    /// May panic, if this [`Unit`] shouldn't be compiled ([`should_run`] returned `false`).
    ///
    /// [`should_run`]: UnitTaskGenerator::should_run
    fn compile_unit(
        &self,
        unit: &Unit,
        id_in_graph: GraphNodeId,
        layout: &dyn ProfileLayout,
    ) -> QuackResult<()> {
        info!("starting compilation of a unit");
        self.set_unit_status(id_in_graph, unit, UnitStatus::InProgress);
        let tasks =
            self.task_generator
                .create_tasks(unit, id_in_graph, &self.graph, layout, self.bcx)?;
        self.compile_unit_with_tasks(unit, id_in_graph, layout, tasks)?;
        self.set_unit_status(id_in_graph, unit, UnitStatus::Finished);
        Ok(())
    }

    /// Compile a single [`Unit`] with its finished tasks.
    #[instrument(skip_all)]
    fn compile_unit_with_tasks(
        &self,
        unit: &Unit,
        unit_id_in_graph: GraphNodeId,
        layout: &dyn ProfileLayout,
        tasks: Vec<multipackage_schema::Task>,
    ) -> QuackResult<()> {
        trace!(?tasks);
        let packages = outputs::collect_packages(unit_id_in_graph, &self.graph)?;
        let schema = multipackage_schema::MultiPackage { packages, tasks };
        trace!(?schema);
        self.compile_unit_with_schema(unit, unit_id_in_graph, layout, schema)
    }

    /// Compile a single [`Unit`] with its finished schema.
    #[instrument(skip_all)]
    fn compile_unit_with_schema(
        &self,
        unit: &Unit,
        unit_id_in_graph: GraphNodeId,
        layout: &dyn ProfileLayout,
        schema: multipackage_schema::MultiPackage,
    ) -> QuackResult<()> {
        let name = unit.package().name();
        let status = (|| {
            let unit_layout = layout.for_dependency(unit_id_in_graph, &self.graph)?;
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
    fn set_unit_status(&self, unit_id_in_graph: GraphNodeId, unit: &Unit, status: UnitStatus) {
        debug_assert!(
            self.unit_statuses.borrow().contains_key(&unit_id_in_graph),
            "missing status for unit {unit:?}, but we set an initial status for every unit"
        );
        self.unit_statuses
            .borrow_mut()
            .insert(unit_id_in_graph, status);
    }
}

#[derive(Debug)]
/// Output of [`run`].
///
/// [`run`]: UnitRunner::run
pub struct CompilationOutput {
    /// Root [`Unit`] and path to its output.
    pub root: (Unit, PathBuf),
    /// To which target have we compiled.
    pub target: CompilationTarget,
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

/// Create an initial map with statuses of all known [`Unit`]s.
///
/// By default all are [`NotStarted`]
///
/// [`NotStarted`]: UnitStatus::NotStarted
fn build_initial_unit_statuses(graph: &UnitGraph) -> HashMap<GraphNodeId, UnitStatus> {
    graph
        .compilation_order()
        .iter()
        .filter_map(|node| node.as_unit().map(|_| (node.id(), UnitStatus::NotStarted)))
        .collect()
}
