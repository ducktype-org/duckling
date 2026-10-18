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
//! Local check
//! -----------
//! For source code inputs the local check is that:
//! - all Duckling files in the package have mtime and ctime (change time) earlier than previous build start timestamp and
//! - the number of Duckling files inside the package remained the same.
//!
//! We need the second check, because the first check cannot detect deleting files.
//! Two checks are sufficient, since the first one detects adding new files.
//!
//! We check ctime, to take renames and moves into account.
//!
//! For units the local check is that artifacts for the unit exist and have mtime earlier than previous build end timestamp.
//!
//! Why two timestamps
//! ------------------
//! Notice that we need two timestamps, since artifacts produced during the compilation have mtime later than the start,
//! and source code files should have mtime earlier than the start.

use std::fs;
use std::path::Path;
use std::time::Duration;

use chrono::{DateTime, Utc};

use crate::QuackResult;
use crate::quackpack::core::AnyPackage;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::{
    CompilationInput, GraphNodeId, UnitGraph, UnitGraphNode, UnitGraphNodeData,
};
use crate::quackpack::core::compile::unit_runner::incremental::{BuildData, IncrementalData};
use crate::quackpack::core::compile::unit_runner::outputs::unit_output;
use crate::quackpack::core::script::Script;
use crate::quackpack::util::dir_iterator::create_dir_iterator;
use crate::util::duckling_consts::{DUCKLING_MODULE_EXT, DUCKLING_SCRIPT_EXT};
use crate::util::path_ops_ext::PathOpsExt;

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
                input.local_check(incremental_data, graph, current_build_data)?
                    && basic_checks(incremental_data, node_id, graph)
            }
            UnitGraphNodeData::Unit(unit) => {
                basic_checks(incremental_data, node_id, graph)
                    && unit.local_check(
                        incremental_data.prev_build_end(),
                        node_id,
                        graph,
                        layout,
                        bcx,
                    )?
            }
        };

        if is_green {
            incremental_data.green_nodes_mut().insert(node_id);
            Ok(true)
        } else {
            Ok(false)
        }
    }
}

/// Helper for [`UnitGraphNode::is_green`].
/// Performs the "basic" checks, namely that:
/// - the node is common with the previous build graph and
/// - all node's dependencies are green.
fn basic_checks(
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
    fn local_check(
        &self,
        incremental_data: &IncrementalData,
        graph: &UnitGraph,
        current_build_data: &mut BuildData,
    ) -> QuackResult<bool> {
        let CompilationInput::PackageSourceCode(identity) = self;
        // Path to recursively check for source code updates.
        let path_to_check = match graph.package_data(*identity).package() {
            AnyPackage::Package(package) => package.source_directory().expect("cannot be global"),
            AnyPackage::Script(script) => match script {
                Script::Standalone(_) => script.script_path(),
                Script::Associated(associated) => {
                    if associated.package().is_global() {
                        script.script_path()
                    } else {
                        // Scripts with associated not global package lie in that package.
                        associated
                            .package()
                            .source_directory()
                            .expect("cannot be global")
                    }
                }
            },
        };
        let SourceCodeInputLocalCheckResult {
            all_mtimes_ok,
            num_src_files,
        } = check_for_updates(path_to_check, incremental_data.prev_build_start())?;
        current_build_data.add_num_src_files(*identity, num_src_files);
        Ok(all_mtimes_ok
            && incremental_data.prev_num_src_files().get(identity).copied() == Some(num_src_files))
    }
}

/// Result of local check for [`CompilationInput::PackageSourceCode`].
/// Contains information if any Duckling file has mtime exceeding given timestampt and the number of found Duckling files.
struct SourceCodeInputLocalCheckResult {
    all_mtimes_ok: bool,
    num_src_files: u32,
}

impl SourceCodeInputLocalCheckResult {
    /// Create a [`SourceCodeInputLocalCheckResult`] for no files at all.
    fn empty() -> Self {
        Self {
            all_mtimes_ok: true,
            num_src_files: 0,
        }
    }

    /// Create a [`SourceCodeInputLocalCheckResult`] for a single file.
    fn for_single_file(mtime_ok: bool) -> Self {
        Self {
            all_mtimes_ok: mtime_ok,
            num_src_files: 1,
        }
    }

    /// Update [`self`] with another [`SourceCodeInputLocalCheckResult`].
    fn update(&mut self, other: SourceCodeInputLocalCheckResult) {
        self.all_mtimes_ok &= other.all_mtimes_ok;
        self.num_src_files += other.num_src_files;
    }
}

/// Helper for [`CompilationInput::local_check`].
/// Recursively checks that all Duckling files inside the given directory (or a single file if `path` is a file)
/// have mtime and ctime earlier than `timestamp`.
/// Also returns the number of encountered Duckling source code files.
fn check_for_updates(
    path: &Path,
    timestamp: &DateTime<Utc>,
) -> QuackResult<SourceCodeInputLocalCheckResult> {
    if path.is_symlink() {
        Ok(SourceCodeInputLocalCheckResult::empty())
    } else if path.is_file() {
        if let Some(extention) = path.extension()
            && [DUCKLING_MODULE_EXT, DUCKLING_SCRIPT_EXT]
                .iter()
                .any(|&e| e == extention)
        {
            let mtime = path.mtime()?;
            // We add one second, since ctime is tracked only with seconds precision, which is too low.
            // Without the addition any rename/move in a second from the build start would be unnoticed.
            let ctime = path.ctime()? + Duration::from_secs(1);
            Ok(SourceCodeInputLocalCheckResult::for_single_file(
                mtime < (*timestamp).into() && ctime < (*timestamp).into(),
            ))
        } else {
            Ok(SourceCodeInputLocalCheckResult::empty())
        }
    } else {
        let mut result = SourceCodeInputLocalCheckResult::empty();
        // We are in a directory.
        for entry in create_dir_iterator(path)? {
            let entry = entry?;
            result.update(check_for_updates(&entry.path(), timestamp)?);
        }
        Ok(result)
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
