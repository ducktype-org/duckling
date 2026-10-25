// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for performing package-level incremental compilation.
//! It is done by serializing some information about the compilation and saving it to the disk (we call it a fingerprint).
//! Next compilation process reads that data and uses it to construct [`IncrementalData`],
//! which is used to decide which units are green (we can reuse previous build artifacts) and which are red (we have to run the unit).
//!
//! The modules are:
//! - [`fingerprint`] --- Contains [`BuildData`], which is precisely the data stored in the fingerprint.
//! - [`graph_checks`] --- Used for determining the common part of the previous and current [`UnitGraph`](crate::quackpack::core::compile::unit::graph::UnitGraph)s.
//!   Only the common nodes can possibly be green.
//! - [`red_green_nodes`] --- Precise rules for whether a node is green or red.
//! - [`serializable_types`] --- [`UnitGraph`](crate::quackpack::core::compile::unit::graph::UnitGraph) is not easily serializable,
//!   so we create use serializable counterparts to it and other types.

use std::collections::{BTreeMap, HashSet};
use std::time::SystemTime;

use crate::quackpack::core::compile::unit::graph::GraphNodeId;
use crate::quackpack::core::identity::Identity;

mod fingerprint;
mod graph_checks;
mod red_green_nodes;
mod serializable_types;

pub use fingerprint::*;

#[derive(Clone, Debug)]
/// All the information necessary for performing package-level incremental compilation.
/// Currently it contains:
/// - [`RedGreenData`] --- a struct containing information about which [`UnitGraphNode`](crate::quackpack::core::compile::unit::graph::UnitGraphNode)s
///   are common with the previous graph and which of those nodes are green (we can reuse build artifacts for them).
///   The former is constructed from the previous fingerprint and for the latter,
///   green nodes are added one by one by [`UnitRunner::compile_unit`](crate::quackpack::core::compile::unit_runner::UnitRunner::compile_unit).
/// - Previous compilation start time (to compare source codes against).
/// - Previous compilation end time (to compare artifacts against).
/// - Mapping from packages to the number of Duckling source code files inside them.
///   This might seem weird, but consult [`red_green_nodes`] module documentation for explanation
///
/// Note:
/// -----
/// [`Self::default`] uses [`SystemTime::UNIX_EPOCH`] as a default time.
/// [`Self::default`] should be called to state that incremental compilation is for some reason impossible.
/// UNIX epoch fits that semantic, since we use the timestamps to make sure that given files were not modified since the timestamp.
/// Obviously in any realistic scenario the files have mtime greater than the epoch.
pub struct IncrementalData {
    red_green_data: RedGreenData,
    prev_build_start: SystemTime,
    prev_build_end: SystemTime,
    prev_number_of_source_files: BTreeMap<Identity, u32>,
}

impl Default for IncrementalData {
    fn default() -> Self {
        Self {
            red_green_data: Default::default(),
            prev_build_start: SystemTime::UNIX_EPOCH,
            prev_build_end: SystemTime::UNIX_EPOCH,
            prev_number_of_source_files: Default::default(),
        }
    }
}

impl IncrementalData {
    /// Construct new [`IncrementalData`].
    pub fn new(
        red_green_data: RedGreenData,
        prev_build_start: SystemTime,
        prev_build_end: SystemTime,
        prev_number_of_source_files: BTreeMap<Identity, u32>,
    ) -> Self {
        Self {
            red_green_data,
            prev_build_start,
            prev_build_end,
            prev_number_of_source_files,
        }
    }

    /// Get the previous build start timestamp.
    pub fn prev_build_start(&self) -> SystemTime {
        self.prev_build_start
    }

    /// Get the previous build end timestamp.
    pub fn prev_build_end(&self) -> SystemTime {
        self.prev_build_end
    }

    /// Get the set of nodes common with the previous graph.
    pub fn common_nodes(&self) -> &HashSet<GraphNodeId> {
        &self.red_green_data.common_nodes
    }

    /// Get the set of green nodes.
    pub fn green_nodes(&self) -> &HashSet<GraphNodeId> {
        &self.red_green_data.green_nodes
    }

    /// Mark a [`UnitGraph`](crate::quackpack::core::compile::unit::graph::UnitGraph) as green.
    pub fn mark_as_green(&mut self, node_id: GraphNodeId) {
        self.red_green_data.green_nodes.insert(node_id);
    }

    /// Get the access to the mapping of previous build packages to the number of Duckling source code files inside them.
    pub fn prev_number_of_source_files(&self) -> &BTreeMap<Identity, u32> {
        &self.prev_number_of_source_files
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
/// Common part between current and previous build [`UnitGraph`]s.
pub struct RedGreenData {
    /// Which packages are the same between builds (in the sense of [`Identity`] and other build-specific information).
    /// Important:
    /// ----------
    /// This does not check if the source code of the package changed, this should be done when considering the [`CompilationInput`](crate::quackpack::core::compile::unit::graph::CompilationInput)
    /// for source code of that package.
    common_packages: HashSet<Identity>,
    /// Which nodes are the same between builds (the nodes are the same and the dependencies are the same).
    /// Those are the nodes for which we potentially can reuse previous build artifacts.
    common_nodes: HashSet<GraphNodeId>,
    /// Subset of [`Self::common_nodes`], for which we have decided that they are green (we can reuse previous build artifacts for them).
    /// This is empty in the beginning and should be gradually filled up by [`UnitRunner::compile_unit`](crate::quackpack::core::compile::unit_runner::UnitRunner::compile_unit).
    green_nodes: HashSet<GraphNodeId>,
}

impl RedGreenData {
    /// Comstruct new [`RedGreenData`], without any green nodes.
    pub fn new(common_packages: HashSet<Identity>, common_nodes: HashSet<GraphNodeId>) -> Self {
        Self {
            common_packages,
            common_nodes,
            green_nodes: HashSet::new(),
        }
    }
}
