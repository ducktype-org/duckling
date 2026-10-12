// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::PathBuf;

use tracing::{debug, instrument, trace};

use crate::QuackResult;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::{CompilationInput, GraphNodeId, UnitGraph};
use crate::quackpack::core::compile::unit::{Unit, UnitType};
use crate::quackpack::core::compile::unit_runner::CompilationOutput;
use crate::quackpack::core::compile::unit_runner::external_libs::gather_external_libraries;

#[instrument(skip_all)]
/// Get the output of this [`run`] pass.
///
/// Right now, this is deterministic (i.e. can be determined from the [`UnitGraph`] and
/// [`ProfileLayout`]), but it may change in the future.
///
/// [`run`]: super::UnitRunner::run
pub fn get_compiler_output(
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<CompilationOutput> {
    let root = graph.root_unit();
    let path = unit_output(root, graph.root_id(), graph, layout, bcx)?;
    Ok(CompilationOutput {
        root: (root.clone(), path),
        target: bcx.compilation_target(),
    })
}

/// Get a path to the output artifact of this [`Unit`].
#[instrument(skip_all)]
pub fn unit_output(
    unit: &Unit,
    unit_id_in_graph: GraphNodeId,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<PathBuf> {
    let is_root = graph.is_root(unit);
    debug!(%is_root, "generating output");
    let out = if is_root {
        layout.root_directory().join(unit.output_file_name(bcx))
    } else {
        let layout = layout.for_dependency(unit_id_in_graph, graph)?;
        layout.root_directory().join(unit.output_file_name(bcx))
    };
    debug!(out = %out.display(), "generated output");
    Ok(out)
}

/// Collect this [`Unit`] and all its dependencies (direct and transitive), as a vector of
/// [`multipackage_schema::Package`].
///
/// Each dependency is present exactly once.
///
/// We currently do this by looking at unit's dependencies which are source code inputs.
#[instrument(skip_all)]
pub fn collect_packages(
    unit_id_in_graph: GraphNodeId,
    graph: &UnitGraph,
) -> QuackResult<Vec<multipackage_schema::Package>> {
    trace!("getting packages");
    let mut packages = vec![];
    for dep in graph.deps_for(unit_id_in_graph) {
        let dep = graph.node_for(*dep);
        let Some(input) = dep.as_input() else {
            continue;
        };
        let CompilationInput::PackageSourceCode(identity) = input;
        let pkg = graph.package_data(*identity);
        packages.push(pkg.multipackage_schema_package(graph)?);
    }
    debug!(?packages);
    Ok(packages)
}

/// Get linker options appropriate for the given `unit`.
///
/// Right now, this:
/// 1. returns `None` if there are no dependencies,
/// 2. creates a [`multipackage_schema::LinkerOptions::RawLinkerArgs`] only for
///    [`UnitType::Dependency`] dependencies (which _should_ be only `.a` files).
#[instrument(skip_all)]
pub fn get_linker_options(
    unit_id_in_graph: GraphNodeId,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<Option<multipackage_schema::LinkerOptions>> {
    trace!("getting linker options");
    let outputs = get_deps_outputs(unit_id_in_graph, graph, layout, bcx)?;
    let external_libs = gather_external_libraries(unit_id_in_graph, graph)
        .into_iter()
        .map(|x| x.to_string());
    let args = outputs
        .into_iter()
        .map(|output| output.display().to_string())
        .chain(external_libs)
        .collect::<Vec<_>>();
    debug!(?args, "raw linker args");
    if args.is_empty() {
        debug!("empty linker options");
        return Ok(None);
    }
    Ok(Some(multipackage_schema::LinkerOptions::RawLinkerArgs(
        args,
    )))
}

/// Collect outputs of dependencies (direct and transitive) of this `unit`.
/// Note:
/// -----
/// This collects outputs of only [`UnitType::Dependency`] dependencies.
#[instrument(skip_all)]
fn get_deps_outputs(
    unit_id_in_graph: GraphNodeId,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<Vec<PathBuf>> {
    trace!("getting output");
    let mut outputs = vec![];
    for dep_id in graph.deps_for(unit_id_in_graph) {
        let Some(dep) = graph.node_for(*dep_id).as_unit() else {
            continue;
        };
        if !matches!(dep.unit_type(), UnitType::Dependency) {
            continue;
        }
        outputs.push(unit_output(dep, *dep_id, graph, layout, bcx)?)
    }
    debug!(?outputs);
    Ok(outputs)
}
