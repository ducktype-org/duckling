// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::collections::BTreeMap;
use std::io::Write;
use std::path::Path;

use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};

use crate::quackpack::core::compile::artifacts_layout::ArtifactsLayout;
use crate::quackpack::core::compile::duckc::Duckc;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit_runner::incremental::serializable_types::SerializableUnitGraph;
use crate::quackpack::core::compile::unit_runner::incremental::{IncrementalData, RedGreenData};
use crate::quackpack::core::identity::Identity;
use crate::util::hash::sha256_string;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext};

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
/// Struct containing information about the current build.
/// This information is stored on the disk, to be used for incremental compilation in the future builds.
pub struct BuildData {
    /// Time of the start of the build.
    build_start: DateTime<Utc>,
    /// Time of the end of the build. If yet unknown (the build is ongoing), set to UNIX epoch.
    build_end: DateTime<Utc>,
    /// Hash of the version of `duckc` used in the build.
    /// It is important to use the verbose version, so that for example changing LLVM version forces recompilation in the incremental setting.
    duckc_version_hash: String,
    /// Unit graph of the build (in an easily serializable version).
    unit_graph: SerializableUnitGraph,
    /// Hash of the profile of the build.
    profile_hash: String,
    /// Artifacts type (which implementation of [`ArtifactsLayout`](crate::quackpack::core::compile::artifacts_layout::ArtifactsLayout) this compilation uses).
    artifacts_type: String,
    /// Mapping from packages to the number of Duckling source code files inside them.
    /// This might seem weird, but consult [`red_green_nodes`](super::red_green_nodes) module documentation for explanation.
    /// We use [`BTreeMap`] to make the fingerprints stable.
    number_of_source_files: BTreeMap<Identity, u32>,
}

impl BuildData {
    /// Get a [`BuildData`] from [`UnitGraph`] and [`Profile`].
    /// Should be used for getting [`BuildData`] for the current build process.
    /// `number_of_source_files` is empty and should be filled up during local checks for source code inputs.
    pub fn for_current_build(
        ctx: &DuckContext,
        graph: &UnitGraph,
        profile: Profile,
        layout: &dyn ArtifactsLayout,
    ) -> QuackResult<Self> {
        let duckc_version = Duckc::get_verbose_version(ctx)?;
        let version_hash = sha256_string(duckc_version);
        let profile_hash = sha256_string(profile.serialize_raw());
        Ok(Self {
            build_start: Utc::now(),
            // Unknown so UNIX epoch.
            build_end: DateTime::UNIX_EPOCH,
            duckc_version_hash: version_hash,
            unit_graph: graph.into(),
            profile_hash,
            artifacts_type: layout.name().to_string(),
            number_of_source_files: BTreeMap::new(),
        })
    }

    /// Get a [`BuildData`] from serialized artifacts.
    /// Should be used for getting [`BuildData`] for the previous build process.
    pub(super) fn from_fingerprint(fingerprint: &Path) -> QuackResult<Self> {
        let fingerprint_content = fingerprint.read().with_context(|| {
            format!(
                "failed to read the previous fingerprint at `{}`",
                fingerprint.display()
            )
        })?;
        let result =
            serde_json::from_slice::<BuildData>(&fingerprint_content).with_context(|| {
                format!(
                    "failed to parse the previous fingerprint at `{}`",
                    fingerprint.display()
                )
            })?;
        result.unit_graph.check_correctness()?;
        Ok(result)
    }

    /// Store a fingerprint with [`BuildData`] on the disk.
    /// Should be called on [`BuildData`] generated for the current build.
    pub(super) fn write_fingerprint(&self, fingerprint: &Path) -> QuackResult<()> {
        let fingerprint_json = serde_json::to_string_pretty(self).with_context_internal(|| {
            format!("failed to convert fingerprint into a JSON string: {self:#?}")
        })?;
        let mut file = fingerprint.touch()?;
        file.set_len(0)
            .with_context(|| format!("failed to truncate `{}`", fingerprint.display()))?;
        file.write_all(fingerprint_json.as_bytes())
            .with_context(|| format!("failed to write fingerprint to `{}`", fingerprint.display()))
    }

    /// Get [`RedGreenData`] (information about which graph nodes are common and their artifacts can be reused)
    /// generated from the current [`BuildData`] and previous build's [`BuildData`].
    pub fn get_incremental_data(&self, other: Self) -> IncrementalData {
        // Different profile, different layouts or different duckc version, we can't reuse anything.
        let red_green_data = if self.profile_hash != other.profile_hash
            || self.artifacts_type != other.artifacts_type
            || self.duckc_version_hash != other.duckc_version_hash
        {
            RedGreenData::default()
        } else {
            self.unit_graph.get_red_green_data(&other.unit_graph)
        };
        IncrementalData::new(
            red_green_data,
            other.build_start,
            other.build_end,
            other.number_of_source_files,
        )
    }

    /// Update the `build_end` timestamp to the current time.
    pub fn set_end_timestamp(&mut self) {
        self.build_end = Utc::now();
    }

    pub fn add_number_of_source_files(&mut self, package: Identity, number_of_source_files: u32) {
        self.number_of_source_files
            .insert(package, number_of_source_files);
    }
}
