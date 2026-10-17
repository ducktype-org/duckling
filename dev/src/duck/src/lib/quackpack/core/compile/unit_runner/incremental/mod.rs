// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for performing package-level incremental compilation.
//! It serializes onto the disk information about the build process (most significantly a representation of the [`UnitGraph`])
//! and uses that information to determine the common subgraph between current and previous [`UnitGraph`]s.
//! Nodes from that subgraph are precisely the nodes for which we can try to do incremental compilation.
//!
//! The information written onto the disk is called a fingerprint and is stored in a struct [`BuildData`].

use std::collections::HashSet;
use std::io::Write;

use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};

use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraph};
use crate::quackpack::core::compile::unit_runner::incremental::serializable_types::SerializableUnitGraph;
use crate::quackpack::core::identity::Identity;
use crate::util::file_locks::LockedFile;
use crate::util::hash::sha256_string;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{QuackResult, QuackResultContext};

mod graph_checks;
mod red_green_nodes;
mod serializable_types;

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
/// Struct containing information about the current build that is stored on the disk,
/// to be used for incremental compilation in the future builds.
pub struct BuildData {
    /// Time of the build.
    time: DateTime<Utc>,
    /// Unit graph of the build (in an easily serializable version).
    unit_graph: SerializableUnitGraph,
    /// Hash of the profile of the build.
    profile_hash: String,
}

impl BuildData {
    /// Get a [`BuildData`] from [`UnitGraph`] and [`Profile`].
    /// Should be used for getting [`BuildData`] for the current build process.
    pub fn for_current_build(graph: &UnitGraph, profile: Profile) -> Self {
        let now = Utc::now();
        let profile_hash = sha256_string(profile.serialize_raw());
        Self {
            time: now,
            unit_graph: graph.into(),
            profile_hash,
        }
    }

    /// Get a [`BuildData`] from serialized artifacts.
    /// Should be used for getting [`BuildData`] for the previous build process.
    pub fn from_fingerprint(fingerprint: &LockedFile) -> QuackResult<Self> {
        let fingerprint_content = fingerprint.path().read().with_context(|| {
            format!(
                "failed to read the previous fingerprint at `{}`",
                fingerprint.path().display()
            )
        })?;
        let result =
            serde_json::from_slice::<BuildData>(&fingerprint_content).with_context(|| {
                format!(
                    "failed to parse the previous fingerprint at `{}`",
                    fingerprint.path().display()
                )
            })?;
        result.unit_graph.check_correctness()?;
        Ok(result)
    }

    /// Store a fingerprint with [`BuildData`] on the disk.
    /// Should be called on [`BuildData`] generated for the current build.
    pub fn write_fingerprint(&self, fingerprint: &LockedFile) -> QuackResult<()> {
        let fingerprint_json = serde_json::to_string_pretty(&self).with_context_internal(|| {
            format!("failed to convert fingerprint into a JSON string: {self:#?}")
        })?;
        fingerprint
            .file()
            .set_len(0)
            .with_context(|| format!("failed to truncate `{}`", fingerprint.path().display()))?;
        fingerprint
            .file()
            .write_all(fingerprint_json.as_bytes())
            .with_context(|| {
                format!(
                    "failed to write fingerprint to `{}`",
                    fingerprint.path().display()
                )
            })
    }

    /// Get [`IncrementalData`] (information about which graph nodes are common and their artifacts can be reused)
    /// generated from the current [`BuildData`] and previous build's [`BuildData`].
    pub fn get_incremental_data(&self, other: &Self) -> IncrementalData {
        // Different profile, we can't reuse anything.
        if self.profile_hash != other.profile_hash {
            return IncrementalData::default();
        }
        // Something went wrong with build times, be conservative and say that we can't reuse anything.
        if self.time < other.time {
            return IncrementalData::default();
        }
        self.unit_graph.get_incremental_data(&other.unit_graph)
    }

    /// Unpack [`self`] into the timestamp.
    pub fn into_timestamp(self) -> DateTime<Utc> {
        self.time
    }

    /// Update the timestamp to the current time.
    pub fn update_timestamp(&mut self) {
        self.time = Utc::now();
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
/// Common part between current and previous build [`UnitGraph`]s.
pub struct IncrementalData {
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
