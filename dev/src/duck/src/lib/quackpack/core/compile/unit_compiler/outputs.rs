use std::convert::Infallible;
use std::ops::ControlFlow;
use std::path::PathBuf;

use itertools::Itertools;
use tracing::{debug, instrument, trace};

use super::CompilationOutput;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::unit_visitor::TryUnitVisitor;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};
use crate::quackpack::core::compile::unit_compiler::external_libs::gather_external_libraries;
use crate::{QuackError, QuackResult};

#[instrument(skip_all)]
/// Get the output of this [`compile`](super::compile) pass.
///
/// Right now, this is deterministic (i.e. can be determined from the [`UnitGraph`] and
/// [`ProfileLayout`]), but it may change in the future.
pub fn get_compiler_output(
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<CompilationOutput> {
    let root = graph.root_unit();
    let path = unit_output(root, graph, layout)?;
    Ok(CompilationOutput {
        root: (root.clone(), path),
    })
}

/// Get a path to the output artifact of this [`Unit`].
#[instrument(skip_all)]
pub fn unit_output(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<PathBuf> {
    let is_root = graph.is_root(unit);
    debug!(%is_root, "generating output");
    let out = if is_root {
        layout.root_directory().join(unit.output_file_name())
    } else {
        let layout = layout.for_dependency(unit, graph)?;
        layout.root_directory().join(unit.output_file_name())
    };
    debug!(out = %out.display(), "generated output");
    Ok(out)
}

/// Collect this [`Unit`] and all its dependencies (direct and transitive), as a vector of
/// [`multipackage_schema::Package`].
///
/// Dependencies appearing in cycles are also included.
///
/// Each dependency is present exactly once.
#[instrument(skip_all)]
pub fn collect_packages(
    unit: &Unit,
    graph: &UnitGraph,
) -> QuackResult<Vec<multipackage_schema::Package>> {
    trace!("getting packages");
    struct PackageVisitor<'graph> {
        graph: &'graph UnitGraph,
        packages: Vec<multipackage_schema::Package>,
    }

    impl TryUnitVisitor for PackageVisitor<'_> {
        type Err = QuackError;

        type Break = Infallible;

        fn try_visit(&mut self, unit: &Unit) -> Result<ControlFlow<Self::Break>, Self::Err> {
            self.packages
                .push(unit.multipackage_schema_package(self.graph)?);
            Ok(ControlFlow::Continue(()))
        }
    }
    let mut visitor = PackageVisitor {
        graph,
        packages: vec![],
    };
    unit.try_accept(&mut visitor, graph)?;
    debug!(packages = ?visitor.packages);
    Ok(visitor.packages)
}

/// Get linker options appropriate for the given `unit`.
///
/// Right now, this:
/// 1. returns `None` if there are no dependencies,
/// 2. creates a [`multipackage_schema::LinkerOptions::RawLinkerArgs`] only for
///    [`ArtifactsType::IsADependencyArtifact`] dependencies (which _should_ be only `.a` files).
#[instrument(skip_all)]
pub fn get_linker_options(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<Option<multipackage_schema::LinkerOptions>> {
    trace!("getting linker options");
    let outputs = get_deps_outputs(unit, graph, layout)?;
    let external_libs = gather_external_libraries(unit, graph)
        .into_iter()
        .map(|x| x.to_string());
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
        .chain(external_libs)
        .join(" ");
    debug!(args = %string, "raw linker args");
    if string.is_empty() {
        debug!("empty linker options");
        return Ok(None);
    }
    Ok(Some(multipackage_schema::LinkerOptions::RawLinkerArgs(
        string,
    )))
}

/// Collect _all_ (including `.a`!) outputs of dependencies (direct and transitive) of this `unit`.
#[instrument(skip_all)]
fn get_deps_outputs(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<Vec<(Unit, PathBuf)>> {
    trace!("getting output");
    struct UnitOutputVisitor<'a> {
        graph: &'a UnitGraph,
        layout: &'a dyn ProfileLayout,
        root: &'a Unit,
        outputs: Vec<(Unit, PathBuf)>,
    }

    impl TryUnitVisitor for UnitOutputVisitor<'_> {
        type Err = QuackError;

        type Break = Infallible;

        fn try_visit(&mut self, unit: &Unit) -> Result<ControlFlow<Self::Break>, Self::Err> {
            if self.root != unit {
                self.outputs
                    .push((unit.clone(), unit_output(unit, self.graph, self.layout)?))
            }
            Ok(ControlFlow::Continue(()))
        }
    }
    let mut visitor = UnitOutputVisitor {
        graph,
        layout,
        root: unit,
        outputs: vec![],
    };
    unit.try_accept(&mut visitor, graph)?;
    debug!(outputs = ?visitor.outputs);
    Ok(visitor.outputs)
}
