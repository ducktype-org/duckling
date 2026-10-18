// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for constructing and saving to the disk fingerprints of the build process.

use std::io::Write;

use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};

use crate::quackpack::core::compile::artifacts_layout::ArtifactsLayout;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit_runner::incremental::serializable_types::SerializableUnitGraph;
use crate::quackpack::core::compile::unit_runner::incremental::{IncrementalData, RedGreenData};
use crate::util::file_locks::LockedFile;
use crate::util::hash::sha256_string;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{QuackResult, QuackResultContext};

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
/// Struct containing information about the current build.
/// This information is stored on the disk, to be used for incremental compilation in the future builds.
pub struct BuildData {
    /// Time of the start of the build.
    build_start: DateTime<Utc>,
    /// Time of the end of the build. If yet unknown (the build is ongoing), set to UNIX epoch.
    build_end: DateTime<Utc>,
    /// Unit graph of the build (in an easily serializable version).
    unit_graph: SerializableUnitGraph,
    /// Hash of the profile of the build.
    profile_hash: String,
    /// Artifacts type (which implementation of [`ArtifactsLayout`](crate::quackpack::core::compile::artifacts_layout::ArtifactsLayout) this compilation uses).
    artifacts_type: String,
}

impl BuildData {
    /// Get a [`BuildData`] from [`UnitGraph`] and [`Profile`].
    /// Should be used for getting [`BuildData`] for the current build process.
    pub fn for_current_build(
        graph: &UnitGraph,
        profile: Profile,
        layout: &dyn ArtifactsLayout,
    ) -> Self {
        let profile_hash = sha256_string(profile.serialize_raw());
        Self {
            build_start: Utc::now(),
            // Unknown so UNIX epoch.
            build_end: DateTime::from_timestamp_nanos(0),
            unit_graph: graph.into(),
            profile_hash,
            artifacts_type: layout.name().to_string(),
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

    /// Get [`RedGreenData`] (information about which graph nodes are common and their artifacts can be reused)
    /// generated from the current [`BuildData`] and previous build's [`BuildData`].
    pub fn get_incremental_data(&self, other: Self) -> IncrementalData {
        // Different profile or different layouts, we can't reuse anything.
        let red_green_data = if self.profile_hash != other.profile_hash
            || self.artifacts_type != other.artifacts_type
        {
            RedGreenData::default()
        } else {
            self.unit_graph.get_red_green_data(&other.unit_graph)
        };
        IncrementalData {
            red_green_nodes: red_green_data,
            prev_build_start: other.build_start,
            prev_build_end: other.build_end,
        }
    }

    /// Update the `build_end` timestamp to the current time.
    pub fn set_end_timestamp(&mut self) {
        self.build_end = Utc::now();
    }
}
