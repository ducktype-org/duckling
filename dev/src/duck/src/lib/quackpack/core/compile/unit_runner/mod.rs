// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! [`UnitRunner`] takes a [`UnitGraph`] and a [`UnitTaskGenerator`] and drives the compilation
//! process using them.

use core::fmt;
use std::cell::RefCell;
use std::collections::HashMap;
use std::io::Write;
use std::path::PathBuf;
use std::process::ExitStatus;
use std::time::SystemTime;

use tracing::{debug, error, info, instrument, trace};

use self::incremental::ExclusiveFingerprintGuard;
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
use crate::quackpack::core::compile::unit_runner::incremental::{BuildData, IncrementalData};
use crate::quackpack::core::compile::unit_task_generator::UnitTaskGenerator;
use crate::quackpack::core::compile::unit_task_generator::default::DefaultTaskGenerator;
use crate::quackpack::core::compile::unit_task_generator::dvm::DvmTaskGenerator;
use crate::util::file_locks::LockedFile;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail};

pub mod external_libs;
pub mod incremental;
pub mod outputs;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// A runner of [`Unit`]s.
pub struct UnitRunner<'duck, 'ctx> {
    graph: UnitGraph,
    task_generator: Box<dyn UnitTaskGenerator>,
    unit_statuses: RefCell<HashMap<GraphNodeId, UnitStatus>>,
    bcx: &'ctx BuildContext<'duck, 'ctx>,
    artifacts_layout: Box<dyn ArtifactsLayout>,
    profile_layout: Box<dyn ProfileLayout>,
}

impl<'duck, 'ctx> UnitRunner<'duck, 'ctx> {
    /// Create a new [`UnitRunner`].
    pub fn new(graph: UnitGraph, bcx: &'ctx BuildContext<'duck, 'ctx>) -> Self {
        let task_generator = bcx.task_generator();
        let unit_statuses = RefCell::new(build_initial_unit_statuses(&graph));
        let artifacts_layout = bcx.artifacts_layout(&graph);
        let profile_layout = artifacts_layout.for_profile(bcx.profile);
        Self {
            graph,
            task_generator,
            unit_statuses,
            bcx,
            artifacts_layout,
            profile_layout,
        }
    }

    /// Get the underlying [`DuckContext`].
    fn ctx(&self) -> &DuckContext {
        self.bcx.pcx.ctx()
    }

    #[instrument(skip_all)]
    /// Drive the compilation with [`UnitRunner`].
    pub fn run(self) -> QuackResult<CompilationOutput> {
        let mut manager = self.profile_layout.fingerprint();
        let guard = manager.lock(self.ctx())?;
        self.run_with_locked_fprint(guard)
    }

    #[instrument(skip_all)]
    /// Drive the compilation with [`UnitRunner`], with locked fingerprint.
    fn run_with_locked_fprint(
        self,
        guard: ExclusiveFingerprintGuard<'_>,
    ) -> QuackResult<CompilationOutput> {
        let (mut build_data, mut incremental_data) = self.manage_fingerprint(&guard)?;
        self.task_generator.pre_compilation(&self.graph, self.bcx)?;
        self.run_all_needed_units(&mut incremental_data, &mut build_data)?;
        let end = SystemTime::now();
        let output = outputs::get_compiler_output(&self.graph, &*self.profile_layout, self.bcx)?;
        // Update timestamp, so all produced artifacts have earlier modification time that the timestamp.
        build_data.set_end_timestamp(end);
        if let Err(e) = guard.write_build_data(build_data) {
            self.ctx().warning(format!(
                "failed to update fingerprint at `{}`: {e}",
                guard.fingerprint_path().display()
            ))?;
        }
        Ok(output)
    }

    #[instrument(skip_all)]
    /// Read the previous build fingerprint, generating [`RedGreenData`] from it.
    /// Saving the new fingerprint is postponed to the end of the whole builds, so this function returns a tuple,
    /// consisting of:
    /// - [`BuildData`] --- the new fingerprint, yet to be written;
    /// - [`IncrementalData`] --- all information necessary for performing package-level incremental compilation;
    ///
    /// Note:
    /// -----
    /// Returns empty [`RedGreenData`] if there was no previous fingerprint found,
    /// parsing it returned an error or profile prohibits incremental.
    fn manage_fingerprint(
        &self,
        guard: &ExclusiveFingerprintGuard<'_>,
    ) -> QuackResult<(BuildData, IncrementalData)> {
        let fprint_path = guard.fingerprint_path();
        let prev_fingerprint_exists = fprint_path.exists();
        let cur_build_data = BuildData::for_current_build(
            self.ctx(),
            &self.graph,
            self.bcx.profile,
            &*self.artifacts_layout,
        )?;
        if !prev_fingerprint_exists || !self.bcx.profile.incremental {
            // It makes no sense to attempt incremental.
            return Ok((cur_build_data, IncrementalData::default()));
        }
        let incremental_data = match guard.read_build_data() {
            Ok(prev_build_data) => {
                debug!(previous_build_data = ?prev_build_data);
                // Previous fingerprint is well-formed, we can use it.
                self.ctx()
                    .info_verbose("successfully loaded previous build fingerprint")?;
                cur_build_data.get_incremental_data(prev_build_data)
            }
            Err(e) => {
                // Previous fingerprint was malformed, so return empty result.
                self.ctx().warning(format!(
                    "previous fingerprint at {} is malformed: {e}",
                    fprint_path.display()
                ))?;
                IncrementalData::default()
            }
        };
        debug!(?incremental_data);
        debug!(?cur_build_data);
        Ok((cur_build_data, incremental_data))
    }

    #[instrument(skip_all)]
    /// Compile all needed [`Unit`]s, as determined by [`should_run`].
    ///
    /// [`should_run`]: UnitTaskGenerator::should_run
    fn run_all_needed_units(
        &self,
        incremental_data: &mut IncrementalData,
        current_build_data: &mut BuildData,
    ) -> QuackResult<()> {
        for node in self.graph.compilation_order() {
            let node_id = node.id();
            // The order is important, we want to run `is_green` for all nodes, since it updates `current_build_data`.
            let is_green = node.is_green(
                &self.graph,
                &*self.profile_layout,
                self.bcx,
                incremental_data,
                current_build_data,
            )?;
            let Some(unit) = node.as_unit() else {
                continue;
            };
            if !self.task_generator.should_run(unit, &self.graph, self.bcx) {
                info!(?unit, "skipping compilation of the unit");
                // Nothing truly depends on this unit (since task_generator skips it, even units having it as dependency do not use its artifacts).
                // So we can safely make it green, potentially making more units green.
                incremental_data.mark_as_green(node_id);
                continue;
            }
            if is_green {
                info!(?unit, "reusing previous build artifacts for the unit");
                self.ctx().info(format!(
                    "reusing previous build artifacts for `{}`",
                    unit.package().name()
                ))?;
                continue;
            }
            match unit.unit_type() {
                UnitType::Binary | UnitType::Library | UnitType::Dependency => {
                    self.compile_unit(unit, node_id)?;
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
    fn compile_unit(&self, unit: &Unit, id_in_graph: GraphNodeId) -> QuackResult<()> {
        info!("starting compilation of a unit");
        self.set_unit_status(id_in_graph, unit, UnitStatus::InProgress);
        let tasks = self.task_generator.create_tasks(
            unit,
            id_in_graph,
            &self.graph,
            &*self.profile_layout,
            self.bcx,
        )?;
        self.compile_unit_with_tasks(unit, id_in_graph, tasks)?;
        self.set_unit_status(id_in_graph, unit, UnitStatus::Finished);
        Ok(())
    }

    /// Compile a single [`Unit`] with its finished tasks.
    #[instrument(skip_all)]
    fn compile_unit_with_tasks(
        &self,
        unit: &Unit,
        unit_id_in_graph: GraphNodeId,
        tasks: Vec<multipackage_schema::Task>,
    ) -> QuackResult<()> {
        trace!(?tasks);
        let packages = outputs::collect_packages(unit_id_in_graph, &self.graph)?;
        let schema = multipackage_schema::MultiPackage { packages, tasks };
        trace!(?schema);
        self.compile_unit_with_schema(unit, unit_id_in_graph, schema)
    }

    /// Compile a single [`Unit`] with its finished schema.
    #[instrument(skip_all)]
    fn compile_unit_with_schema(
        &self,
        unit: &Unit,
        unit_id_in_graph: GraphNodeId,
        schema: multipackage_schema::MultiPackage,
    ) -> QuackResult<()> {
        let name = unit.package().name();
        let status = (|| {
            let unit_layout = self
                .profile_layout
                .for_dependency(unit_id_in_graph, &self.graph)?;
            let builder =
                self.finished_builder_for_layout_and_profile(&*unit_layout, self.bcx.profile);
            let _lock = unit_layout.acquire_lock(self.ctx())?;
            let locked_manifest_file = unit_layout
                .dependency_json(self.ctx())
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
        let mut builder = Duckc::new(self.ctx()).process_builder();
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
        self.ctx().info(format!("compiling `{name}`..."))?;
        self.ctx().info_verbose(format!("Running `{}`", builder))?;
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
