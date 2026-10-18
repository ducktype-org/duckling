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

use std::collections::HashSet;

use chrono::{DateTime, Utc};

use crate::quackpack::core::compile::unit::graph::GraphNodeId;
use crate::quackpack::core::identity::Identity;

mod fingerprint;
mod graph_checks;
mod red_green_nodes;
mod serializable_types;

pub use fingerprint::BuildData;

#[derive(Clone, Debug, Default)]
/// All the information necessary for performing package-level incremental compilation.
/// Currently it contains:
/// - [`RedGreenData`] --- a struct containing information about which [`UnitGraphNode`](crate::quackpack::core::compile::unit::graph::UnitGraphNode)s
///   are common with the previous graph and which of those nodes are green (we can reuse build artifacts for them).
///   The first former is constructed from the previous fingerprint and for the latter,
///   green nodes are added one by one by [`UnitRunner::compile_unit`](crate::quackpack::core::compile::unit_runner::UnitRunner::compile_unit).
/// - Previous compilation start time (to compare source codes against).
/// - Previous compilation end time (to compare artifacts against).
///
/// Note:
/// -----
/// Default for [`DateTime`] is UNIX epoch.
/// [`Self::default`] should be called to state that incremental compilation is for some reason impossible.
/// UNIX epoch fits that semantic, since we use the timestamps to make sure that given files were not modified since the timestamp.
/// Obviously in any realistic scenario the files have mtime greater than the epoch.
pub struct IncrementalData {
    pub red_green_nodes: RedGreenData,
    pub prev_build_start: DateTime<Utc>,
    pub prev_build_end: DateTime<Utc>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
/// Common part between current and previous build [`UnitGraph`]s.
pub struct RedGreenData {
    /// Which packages are the same between builds (in the sense of [`Identity`] and other build-specific information).
    /// Important:
    /// ----------
    /// This does not check if the source code of the package changed, this should be done when considering the [`CompilationInput`](crate::quackpack::core::compile::unit::graph::CompilationInput)
    /// for source code of that package.
    pub common_packages: HashSet<Identity>,
    /// Which nodes are the same between builds (the nodes are the same and the dependencies are the same).
    /// Those are the nodes for which we potentially can reuse previous build artifacts.
    pub common_nodes: HashSet<GraphNodeId>,
    /// Subset of [`Self::common_nodes`], for which we have decided that they are green (we can reuse previous build artifacts for them).
    /// This is empty in the beginning and should be gradually filled up by [`UnitRunner::compile_unit`](crate::quackpack::core::compile::unit_runner::UnitRunner::compile_unit).
    pub green_nodes: HashSet<GraphNodeId>,
}
