//! Schemas used when communicating with a registry.
use crate::quackpack::{core::Version, schemas::OneEntryMap};
use std::collections::HashMap;

use serde::{Deserialize, Serialize};

pub type Dependencies = HashMap<String, Dependency>;

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Registry manifest schema.
pub struct Manifest {
    /// Package's metadata.
    pub metadata: Metadata,
    /// Package's dependencies.
    pub dependencies: Dependencies,
    /// Package's dev dependencies.
    pub dev_dependencies: Dependencies,
    /// Package's features.
    pub features: HashMap<String, Vec<String>>,
    /// Package's profiles.
    pub profiles: HashMap<String, Profile>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Registry metadata schema.
pub struct Metadata {
    /// Version of the package.
    pub version: Version,
    /// Package's authors.
    pub authors: Vec<String>,
    /// Package's license.
    pub license: String,
    /// Package's name.
    pub name: String,
    /// Package's description.
    pub description: String,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Single dependency of a package.
pub struct Dependency {
    /// Possible versions of the dependency.
    pub version: Vec<Version>,
    /// Source of the dependency.
    pub source: DependencySource,
    /// Dependency's features.
    pub features: Vec<DependencyFeature>,
    /// Is this dependency pinned to a specific version.
    pub pinned: bool,
    /// Conditions required for enabling this dependency.
    pub conditions: DependencyCondition,
    /// Whether it's aliased.
    pub is_alias_for: Option<String>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Source of the dependency.
pub struct DependencySource {
    /// Python-compatibility artefact.
    pub inner: SourceInner,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(tag = "type")]
#[serde(rename_all = "snake_case")]
/// Actual source of the dependency.
pub enum SourceInner {
    /// A registry dependency...
    Registry {
        /// ...from this url.
        registry_url: String,
    },
    /// A local dependency.
    Local {
        /// Absolute path to the dependency.
        absolute_dir_root: String,
        /// Path specified in the manifest.
        dir_entry_in_manifest: String,
    },
    /// A git dependency.
    Git {
        /// Url to a git repository.
        git_url: String,
        /// Specified commit.
        commit: Option<String>,
        /// Specified tag.
        tag: Option<String>,
        /// Specified branch.
        branch: Option<String>,
    },
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(untagged)]
/// A dependency feature.
pub enum DependencyFeature {
    /// Just a feature, without conditions.
    Simple(String),
    /// Single feature + its conditions.
    Detailed(OneEntryMap<String, DependencyCondition>),
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct Profile {
    pub opt_level: Option<OptLevel>,
    pub dvm_bytecode: Option<bool>,
    pub incremental: Option<bool>,
    pub c_std: Option<bool>,
    pub inherits: Option<String>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(untagged)]
pub enum OptLevel {
    Zero,
    One,
    Two,
    Three,
    S,
    Z,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
/// Conditions of a dependency.
pub struct DependencyCondition {
    /// Required root package features.
    pub package_features: Option<Vec<String>>,
}
