// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Logic for deciding whether a node is green (we can reuse previous build artifacts) or red (we cannot do that).
//! Node is green if:
//! 1. It was present in the previous build graph.
//! 2. All of its dependencies are green.
//! 3. Node-specific check passed.
//!
//! Node-specific check
//! -------------------
//! For source code inputs the check is that:
//! - all Duckling files in the package have mtime and ctime (change time) earlier than previous build start timestamp and
//! - the number of Duckling files inside the package remained the same.
//!
//! We need the second check, because the first check cannot detect deleting files.
//! Two checks are sufficient, since the first one detects adding new files.
//!
//! We check ctime, to take renames and moves into account.
//!
//! For units the check is that artifacts for the unit exist and have mtime and ctime earlier than previous build end timestamp.
//!
//! Why two timestamps
//! ------------------
//! Notice that we need two timestamps, since artifacts produced during the compilation have mtime later than the start,
//! and source code files should have mtime earlier than the start.

use std::convert::Infallible;
use std::ops::ControlFlow;
use std::path::Path;
use std::time::SystemTime;

use crate::QuackResult;
<<<<<<< HEAD:dev/src/duck/src/lib/quackpack/core/compile/unit_runner/incremental/red_green_nodes.rs
use crate::quackpack::core::TryFileVisitor;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::{
    CompilationInput, GraphNodeId, UnitGraph, UnitGraphNode, UnitGraphNodeData,
};
use crate::quackpack::core::compile::unit_runner::incremental::{BuildData, IncrementalData};
use crate::quackpack::core::compile::unit_runner::outputs::unit_output;
use crate::util::path_ops::{change_time, modification_time};
=======
use crate::core::AnyPackage;
use crate::core::compile::BuildContext;
use crate::core::compile::artifacts_layout::ProfileLayout;
use crate::core::compile::unit::Unit;
use crate::core::compile::unit::graph::{
    CompilationInput, GraphNodeId, UnitGraph, UnitGraphNode, UnitGraphNodeData,
};
use crate::core::compile::unit_runner::incremental::{BuildData, IncrementalData};
use crate::core::compile::unit_runner::outputs::unit_output;
use crate::core::script::Script;
use crate::util::duckling_consts::{DUCKLING_MODULE_EXT, DUCKLING_SCRIPT_EXT};
use crate::util::filesystem::{change_time, create_dir_iterator, modification_time};
>>>>>>> main:dev/src/duck/quackpack/src/core/compile/unit_runner/incremental/red_green_nodes.rs

impl UnitGraphNode {
    /// Main logic for checking whether a [`UnitGraphNode`] is green or red.
    ///
    /// Important
    /// ---------
    /// This function also updates `current_build_data` with the number of Duckling source code files for the appropriate packages.
    pub fn is_green(
        &self,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
        incremental_data: &mut IncrementalData,
        current_build_data: &mut BuildData,
    ) -> QuackResult<bool> {
        let node_id = self.id();

        // For source code inputs we first do the local check
        // (to update `current_build_data` with the number of Duckling files for all packages).
        // For units we first do basic checks, which are probably faster than the local check and realistically fail far more often.
        let is_green = match self.node() {
            UnitGraphNodeData::Input(input) => {
                input.node_specific_check(incremental_data, graph, current_build_data)?
                    && meets_trivial_green_preconditions(incremental_data, node_id, graph)
            }
            UnitGraphNodeData::Unit(unit) => {
                meets_trivial_green_preconditions(incremental_data, node_id, graph)
                    && unit.node_specific_check(
                        incremental_data.prev_build_end(),
                        node_id,
                        graph,
                        layout,
                        bcx,
                    )?
            }
        };

        if is_green {
            incremental_data.mark_as_green(node_id);
            Ok(true)
        } else {
            Ok(false)
        }
    }
}

/// Helper for [`UnitGraphNode::is_green`].
/// Performs the obvious checks, namely that:
/// - the node is common with the previous build graph and
/// - all node's dependencies are green.
fn meets_trivial_green_preconditions(
    incremental_data: &IncrementalData,
    node_id: GraphNodeId,
    graph: &UnitGraph,
) -> bool {
    // If node was not in the previous build then it is red.
    if !incremental_data.common_nodes().contains(&node_id) {
        return false;
    }

    // Check that all dependencies are green.
    graph
        .deps_for(node_id)
        .iter()
        .all(|dep_id| incremental_data.green_nodes().contains(dep_id))
}

impl CompilationInput {
    /// Local check for [`CompilationInput`].
    /// Currently checks for [`CompilationInput::PackageSourceCode`] that all Duckling files of the package were not updated.
    fn node_specific_check(
        &self,
        incremental_data: &IncrementalData,
        graph: &UnitGraph,
        current_build_data: &mut BuildData,
    ) -> QuackResult<bool> {
        let CompilationInput::PackageSourceCode(identity) = self;
        let mut check_visitor = InputCheckVisitor::new(incremental_data.prev_build_start());
        graph
            .package_data(*identity)
            .package()
            .try_visit_all_source_files(&mut check_visitor)?;
        current_build_data
            .add_number_of_source_files(*identity, check_visitor.number_of_source_files);
        Ok(check_visitor.all_mtimes_ok
            && incremental_data
                .prev_number_of_source_files()
                .get(identity)
                .copied()
                == Some(check_visitor.number_of_source_files))
    }
}

/// Implementor of [`TryFileVisitor`] used by [`CompilationInput::node_specific_check`].
/// Checks if a single Duckling file has mtime and ctime earlier than a given timestamp,
/// and counts the number of visited Duckling files.
struct InputCheckVisitor {
    all_mtimes_ok: bool,
    number_of_source_files: u32,
    timestamp: SystemTime,
}

impl InputCheckVisitor {
    /// Create new [`InputCheckVisitor`].
    fn new(timestamp: SystemTime) -> Self {
        Self {
            all_mtimes_ok: true,
            number_of_source_files: 0,
            timestamp,
        }
    }
}

impl TryFileVisitor for InputCheckVisitor {
    type Break = Infallible;

    fn visit(&mut self, path: &Path) -> QuackResult<ControlFlow<Self::Break>> {
        let mtime = modification_time(path)?;
        let ctime = change_time(path)?;
        let mtime_ok = mtime < self.timestamp && ctime < self.timestamp;
        self.all_mtimes_ok &= mtime_ok;
        self.number_of_source_files += 1;
        Ok(ControlFlow::Continue(()))
    }
}

impl Unit {
    /// Local check for [`Unit`].
    /// Checks that artifacts for the unit exist and have mtime earlier than timestamp.
    fn node_specific_check(
        &self,
        previous_timestamp: SystemTime,
        unit_id_in_graph: GraphNodeId,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<bool> {
        let artifacts_file = unit_output(self, unit_id_in_graph, graph, layout, bcx)?;
        if artifacts_file.exists() {
            let mtime = modification_time(&artifacts_file)?;
            let ctime = change_time(artifacts_file)?;
            Ok(mtime < previous_timestamp && ctime < previous_timestamp)
        } else {
            Ok(false)
        }
    }
}
