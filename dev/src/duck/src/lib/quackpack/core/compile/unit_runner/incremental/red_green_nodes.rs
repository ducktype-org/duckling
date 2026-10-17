// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Logic for deciding whether a node is green (we can reuse previous build artifacts) or red (we cannot do that).
//! Node is green if:
//! 1. It was present in the previous build graph.
//! 2. All of its dependencies are green.
//! 3. Local check passed.
//!
//! The local check is:
//! - for source code inputs, checking that all Duckling files in the package have mtime earlier than previous build timestamp;
//! - for units, checking that artifacts for the unit exist and have mtime earlier than previous build timestamp.

use std::fs;
use std::path::Path;

use chrono::{DateTime, Utc};

use crate::QuackResult;
use crate::quackpack::core::AnyPackage;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::{
    CompilationInput, GraphNodeId, UnitGraph, UnitGraphNode, UnitGraphNodeData,
};
use crate::quackpack::core::compile::unit_runner::incremental::IncrementalData;
use crate::quackpack::core::compile::unit_runner::outputs::unit_output;
use crate::quackpack::core::script::Script;
use crate::quackpack::util::dir_iterator::create_dir_iterator;
use crate::util::duckling_consts::{DUCKLING_MODULE_EXT, DUCKLING_SCRIPT_EXT};

impl UnitGraphNode {
    /// Main logic for checking whether a [`UnitGraphNode`] is green or red.
    pub fn is_green(
        &self,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
        previous_timestamp: &DateTime<Utc>,
        incremental_data: &mut IncrementalData,
    ) -> QuackResult<bool> {
        let node_id = self.id();

        // If node was not in the previous build then it is red.
        if !incremental_data.common_nodes.contains(&node_id) {
            return Ok(false);
        }

        // Check that all dependencies are green first.
        let all_deps_green = graph
            .deps_for(node_id)
            .iter()
            .all(|dep_id| incremental_data.green_nodes.contains(dep_id));
        if !all_deps_green {
            return Ok(false);
        }

        // Perform the additional local check.
        let local_check = match self.node() {
            UnitGraphNodeData::Input(input) => input.local_check(previous_timestamp, graph)?,
            UnitGraphNodeData::Unit(unit) => {
                unit.local_check(previous_timestamp, node_id, graph, layout, bcx)?
            }
        };

        if local_check {
            incremental_data.green_nodes.insert(node_id);
            Ok(true)
        } else {
            Ok(false)
        }
    }
}

impl CompilationInput {
    /// Local check for [`CompilationInput`].
    /// Currently checks for [`CompilationInput::PackageSourceCode`] that all Duckling files of the package were not updated.
    fn local_check(
        &self,
        previous_timestamp: &DateTime<Utc>,
        graph: &UnitGraph,
    ) -> QuackResult<bool> {
        let CompilationInput::PackageSourceCode(identity) = self;
        // Path to recursively check for source code updates.
        let path_to_check = match graph.package_data(*identity).package() {
            AnyPackage::Package(package) => package.root_directory(),
            AnyPackage::Script(script) => match script {
                Script::Standalone(_) => script.script_path(),
                Script::Associated(associated) => {
                    if associated.package().is_global() {
                        script.script_path()
                    } else {
                        // Scripts with associated not global package lie in that package.
                        associated.package().root_directory()
                    }
                }
            },
        };
        check_for_updates(path_to_check, previous_timestamp)
    }
}

/// Helper for [`CompilationInput::local_check`].
/// Recursively checks that all Duckling files inside the given directory (or a single file if `path` is a file)
/// have mtime earlier than `timestamp`.
fn check_for_updates(path: &Path, timestamp: &DateTime<Utc>) -> QuackResult<bool> {
    if path.is_file() {
        if let Some(extention) = path.extension()
            && [DUCKLING_MODULE_EXT, DUCKLING_SCRIPT_EXT]
                .iter()
                .any(|&e| e == extention)
        {
            let mtime = fs::metadata(path)?.modified()?;
            Ok(mtime < (*timestamp).into())
        } else {
            Ok(true)
        }
    } else {
        // We are in a directory.
        for entry in create_dir_iterator(path)? {
            let entry = entry?;
            if !check_for_updates(&entry.path(), timestamp)? {
                return Ok(false);
            }
        }
        Ok(true)
    }
}

impl Unit {
    /// Local check for [`Unit`].
    /// Checks that artifacts for the unit exist and have mtime earlier than timestamp.
    fn local_check(
        &self,
        previous_timestamp: &DateTime<Utc>,
        unit_id_in_graph: GraphNodeId,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<bool> {
        let artifacts_file = unit_output(self, unit_id_in_graph, graph, layout, bcx)?;
        if artifacts_file.exists() {
            let mtime = fs::metadata(artifacts_file)?.modified()?;
            Ok(mtime < (*previous_timestamp).into())
        } else {
            Ok(false)
        }
    }
}
