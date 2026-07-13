//! [`Executor`] takes a [`UnitGraph`] and compiles it, according to its strategy.

use std::collections::HashSet;
use std::fmt::{self, Debug};
use std::io::Write;
use std::path::PathBuf;
use std::process::ExitStatus;

pub mod debug_executor;
pub mod dvm_executor;

#[cfg(test)]
mod tests;

use itertools::Itertools;
use tracing::debug;

use self::debug_executor::DebugExecutor;
use self::dvm_executor::DvmExecutor;
use super::BuildContext;
use super::artifacts_layout::{DependencyLayout, ProfileLayout};
use super::duckc::process_builder::DuckcSubcommand;
use super::duckc::{Duckc, multipackage_schema, process_builder};
use super::profiles::Profile;
use super::unit::Unit;
use super::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::ArtifactsType;
use crate::util::file_locks::LockedFile;
use crate::{QuackResult, QuackResultContext, qp_bail};

/// A generic duckc driver.
pub trait Executor: Debug {
    /// Try to compile the given [`UnitGraph`].
    fn compile(&self, graph: UnitGraph, bcx: &BuildContext<'_, '_>) -> QuackResult<ExecutorOutput>;
}

#[derive(Debug)]
/// Output of [`Executor::compile`].
pub struct ExecutorOutput {
    /// Root [`Unit`] and path to its output.
    pub root: (Unit, PathBuf),
}

impl BuildContext<'_, '_> {
    /// Get an appropriate executor.
    pub fn executor(&self) -> Box<dyn Executor> {
        if self.profile.dvm_bytecode {
            return Box::new(DvmExecutor);
        }
        Box::new(DebugExecutor)
    }
}

/// Collect this [`Unit`] and all its dependencies (direct and transitive), as a vector of
/// [`multipackage_schema::Package`].
///
/// Dependencies appearing in cycles are also included.
///
/// Each dependency is present exactly once.
///
/// Right now, order of the vector is indeterministic.
/// (To be precise, it's a normal DFS order).
pub(crate) fn collect_packages(
    unit: &Unit,
    graph: &UnitGraph,
) -> Vec<multipackage_schema::Package> {
    let mut result = vec![];
    let mut visited = HashSet::new();
    fn dfs(
        current: &Unit,
        graph: &UnitGraph,
        result: &mut Vec<multipackage_schema::Package>,
        visited: &mut HashSet<Unit>,
    ) {
        if visited.contains(current) {
            return;
        }
        visited.insert(current.clone());
        result.push(current.multipackage_schema_package(graph));
        for dep_id in current.deps_sorted_by_unit_id() {
            let dep = graph.unit_for(*dep_id);
            dfs(dep, graph, result, visited);
        }
    }
    dfs(unit, graph, &mut result, &mut visited);
    result
}

/// Get linker options appropriate for the given `unit`.
///
/// Right now, this:
/// 1. returns `None` if there are no dependencies,
/// 2. creates a [`multipackage_schema::LinkerOptions::RawLinkerArgs`] only for
///    [`ArtifactsType::IsADependencyArtifact`] dependencies (which _should_ be only `.a` files).
pub(crate) fn get_linker_options(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &ProfileLayout,
) -> Option<multipackage_schema::LinkerOptions> {
    let outputs = get_deps_outputs(unit, graph, layout);
    if outputs.is_empty() {
        return None;
    }
    let string = outputs
        .into_iter()
        .filter_map(|(unit, output)| {
            if unit.artifacts_type() == ArtifactsType::IsADependencyArtifact {
                Some(output)
            } else {
                None
            }
        })
        .map(|output| output.display().to_string())
        .join(" ");
    debug!("raw linker args are `{string}`");
    if string.is_empty() {
        return None;
    }
    Some(multipackage_schema::LinkerOptions::RawLinkerArgs(string))
}

/// Collect _all_ (including `.a`!) outputs of dependencies (direct and transitive) of this `unit`.
pub(crate) fn get_deps_outputs(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &ProfileLayout,
) -> Vec<(Unit, PathBuf)> {
    let mut result = vec![];
    let mut visited = HashSet::new();
    // Note that this DFS is different than the one above: we skip the root.
    fn dfs(
        current: &Unit,
        graph: &UnitGraph,
        layout: &ProfileLayout,
        result: &mut Vec<(Unit, PathBuf)>,
        visited: &mut HashSet<Unit>,
    ) {
        for dep_id in current.deps_sorted_by_unit_id() {
            let dep = graph.unit_for(*dep_id);
            if visited.contains(dep) {
                continue;
            }
            visited.insert(dep.clone());
            result.push((dep.clone(), unit_output(dep, graph, layout)));
            dfs(dep, graph, layout, result, visited);
        }
    }
    dfs(unit, graph, layout, &mut result, &mut visited);
    result
}

/// Get a path to the output artifact of this `unit`.
pub(crate) fn unit_output(unit: &Unit, graph: &UnitGraph, layout: &ProfileLayout) -> PathBuf {
    let out = if graph.is_root(unit) {
        layout.root_directory().join(unit.output_file_name())
    } else {
        let layout = layout.for_dependency(&unit.unique_name());
        layout.root_directory().join(unit.output_file_name())
    };
    debug!(unit = ?unit, out = %out.display(), "generating output");
    out
}

/// Write a manifest into a file.
pub(crate) fn write_schema(
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
pub(crate) fn finished_builder_for_layout_and_profile(
    bcx: &BuildContext<'_, '_>,
    layout: &DependencyLayout,
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
pub(crate) fn compile_and_print(
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

/// Compile a single [`Unit`] with its finished schema.
pub(crate) fn compile_single_unit_with_schema(
    unit: &Unit,
    layout: &ProfileLayout,
    bcx: &BuildContext<'_, '_>,
    schema: multipackage_schema::MultiPackage,
) -> QuackResult<()> {
    let name = unit.root_package().package().name();
    let status = (|| {
        let unit_layout = layout.for_dependency(&unit.unique_name());
        let builder = finished_builder_for_layout_and_profile(bcx, &unit_layout, &bcx.profile);
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
        qp_bail!("failed to compile `{name}`")
    }
    Ok(())
}

/// Compile a single [`Unit`] with its finished tasks.
pub(crate) fn compile_single_unit_with_tasks(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &ProfileLayout,
    bcx: &BuildContext<'_, '_>,
    tasks: Vec<multipackage_schema::Task>,
) -> QuackResult<()> {
    let packages = collect_packages(unit, graph);
    let schema = multipackage_schema::MultiPackage { packages, tasks };
    compile_single_unit_with_schema(unit, layout, bcx, schema)
}
