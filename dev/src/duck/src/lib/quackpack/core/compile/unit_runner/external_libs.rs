// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various functionalities for interacting with external libraries specified in the manifest.

use std::collections::HashMap;
use std::convert::Infallible;
use std::ops::ControlFlow;

use tracing::instrument;

use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraph, UnitGraphNode};
use crate::quackpack::core::compile::unit::graph_visitor::GraphVisitor;
use crate::{QuackResult, StrId, qp_bail};

#[derive(Debug)]
/// Marker struct indicating that we have found an external library.
///
/// It may not make sense to have explicit [`links`](ExternalLibrariesFound::links) field, but
/// otherwise it's hard for the caller to get the links field, since the [`Unit`] only stores an
/// [`Option`].
///
/// This struct has a logic invariant that
/// ```rust,ignore (illustrative)
/// self.unit.root_package().package().manifest().build_options().links == Some(self.links)
/// ```
/// but allows caller to get `links` without going through an [`Option`].
pub struct ExternalLibrariesFound {
    /// [`Unit`] which links against the library.
    pub unit: Unit,
    /// The said external library.
    pub links: StrId,
}

#[instrument(skip_all)]
/// Check whether subtree rooted at `unit` links against external libraries.
///
/// This function returns _any_ [`Unit`] with external library, if there is one.
///
/// In case there are multiple such [`Unit`]s, it's not guaranteed which one is returned.
pub fn has_external_libraries(
    unit_graph_id: GraphNodeId,
    graph: &UnitGraph,
) -> Option<ExternalLibrariesFound> {
    struct ExternalLibsVisitor;

    impl GraphVisitor for ExternalLibsVisitor {
        type Break = ExternalLibrariesFound;

        fn visit(&mut self, node: &UnitGraphNode) -> ControlFlow<Self::Break> {
            let Some(unit) = node.as_unit() else {
                return ControlFlow::Continue(());
            };
            let links = &unit.package().manifest().build_options().links;
            match links {
                Some(links) => ControlFlow::Break(ExternalLibrariesFound {
                    unit: unit.clone(),
                    links: *links,
                }),
                None => ControlFlow::Continue(()),
            }
        }
    }
    graph
        .node_for(unit_graph_id)
        .accept(&mut ExternalLibsVisitor {}, graph)
}

#[instrument(skip_all)]
/// Validate external libraries against which we are linking.
/// Right now this checks for duplicates of the same library.
pub fn validate_external_libraries(
    unit_graph_id: GraphNodeId,
    graph: &UnitGraph,
) -> QuackResult<()> {
    #[derive(Default)]
    /// Map external lib -> Unit linking with it.
    struct DuplicateLibsVisitor(HashMap<StrId, Unit>);

    impl GraphVisitor for DuplicateLibsVisitor {
        /// (Dup A, Dup B, What)
        type Break = (Unit, Unit, StrId);

        fn visit(&mut self, node: &UnitGraphNode) -> ControlFlow<Self::Break> {
            let Some(unit) = node.as_unit() else {
                return ControlFlow::Continue(());
            };
            let links = unit
                .package()
                .manifest()
                .build_options()
                .links
                .as_ref()
                .copied();
            // No external libs.
            let Some(links) = links else {
                return ControlFlow::Continue(());
            };
            let maybe_previous = self.0.insert(links, unit.clone());
            // Not a duplicate.
            let Some(previous) = maybe_previous else {
                return ControlFlow::Continue(());
            };
            ControlFlow::Break((previous.clone(), unit.clone(), links))
        }
    }
    let unit = graph.node_for(unit_graph_id);
    let Some((dup_a, dup_b, links)) = unit.accept(&mut DuplicateLibsVisitor::default(), graph)
    else {
        return Ok(());
    };
    qp_bail!(
        "two different packages {} and {} both link against external library `{links}`",
        dup_a.pkg_descriptive_name(),
        dup_b.pkg_descriptive_name()
    )
}

#[instrument(skip_all)]
/// Gather all external libraries of the subgraph rooted at a given node in the [`UnitGraph`].
pub fn gather_external_libraries(unit_graph_id: GraphNodeId, graph: &UnitGraph) -> Vec<StrId> {
    #[derive(Default)]
    struct ExternalLibsCollector(Vec<StrId>);

    impl GraphVisitor for ExternalLibsCollector {
        type Break = Infallible;

        fn visit(&mut self, node: &UnitGraphNode) -> ControlFlow<Self::Break> {
            let Some(unit) = node.as_unit() else {
                return ControlFlow::Continue(());
            };
            let links = unit
                .package()
                .manifest()
                .build_options()
                .links
                .as_ref()
                .copied();
            if let Some(links) = links {
                self.0.push(links);
            };
            ControlFlow::Continue(())
        }
    }
    let mut visitor = ExternalLibsCollector::default();
    let node = graph.node_for(unit_graph_id);
    node.accept(&mut visitor, graph);
    visitor.0
}
