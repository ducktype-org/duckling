//! [`Executor`] takes a [`UnitGraph`] and compiles it, according to its strategy.

use std::collections::HashSet;
use std::fmt::Debug;
use std::io::Write;
use std::path::PathBuf;

pub mod debug_executor;

use itertools::Itertools;
use tracing::debug;

use super::BuildContext;
use super::artifacts_layout::ProfileLayout;
use super::duckc::multipackage_schema;
use super::unit::Unit;
use super::unit::graph::UnitGraph;
use crate::util::file_locks::LockedFile;
use crate::{QuackResult, QuackResultContext};

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

/// Collect recursively all packages below `unit`.
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
        for dep_id in current.deps_by_unit_id() {
            let dep = graph.unit_for(*dep_id);
            dfs(dep, graph, result, visited);
        }
    }
    dfs(unit, graph, &mut result, &mut visited);
    result
}

/// Get linker options appropriate for the given `unit`.
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
        .map(|output| output.display().to_string())
        .join(" ");
    debug!("raw linker args are `{string}`");
    Some(multipackage_schema::LinkerOptions::RawLinkerArgs(string))
}

/// Collect outputs of _all_ (including `.a`!) dependencies below this `unit`.
pub(crate) fn get_deps_outputs(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &ProfileLayout,
) -> Vec<PathBuf> {
    let mut result = vec![];
    let mut visited = HashSet::new();
    // Note that this DFS is different than the one above: we skip the root.
    fn dfs(
        current: &Unit,
        graph: &UnitGraph,
        layout: &ProfileLayout,
        result: &mut Vec<PathBuf>,
        visited: &mut HashSet<Unit>,
    ) {
        for dep_id in current.deps_by_unit_id() {
            let dep = graph.unit_for(*dep_id);
            if visited.contains(dep) {
                continue;
            }
            visited.insert(dep.clone());
            result.push(unit_output(dep, graph, layout));
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
pub(crate) fn write_manifest(
    manifest: multipackage_schema::MultiPackage,
    locked_manifest_file: &LockedFile,
) -> QuackResult<()> {
    let manifest_json = serde_json::to_string_pretty(&manifest)
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
        .context("failed to write to manifest.json")?;
    Ok(())
}
