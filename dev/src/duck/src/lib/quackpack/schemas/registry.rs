//! Schemas used when communicating with a registry.
use std::collections::HashMap;

use serde::{Deserialize, Serialize};

use crate::quackpack::core::Version;
use crate::quackpack::schemas::OneEntryMap;

pub type Dependencies = Vec<Dependency>;

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(rename_all = "kebab-case")]
/// Registry manifest schema.
pub struct Manifest {
    /// Package's metadata.
    pub metadata: Metadata,
    /// All direct package's dependencies.
    pub dependencies: Dependencies,
    /// Package's features.
    pub features: HashMap<String, Vec<String>>,
    /// Package's profiles.
    pub profiles: HashMap<String, Profile>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(rename_all = "kebab-case")]
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
#[serde(rename_all = "kebab-case")]
/// Single dependency of a package.
pub struct Dependency {
    /// Name of the dependency.
    pub name: String,
    /// Possible versions of the dependency.
    pub version: Vec<Version>,
    /// Source of the dependency.
    pub source: DependencySource,
    /// Type of this dependency.
    pub kind: DependencyKind,
    /// Dependency's features.
    pub features: Vec<DependencyFeature>,
    /// Is this dependency pinned to a specific version.
    pub pinned: bool,
    /// Conditions required for enabling this dependency.
    pub conditions: DependencyCondition,
    /// Whether it's aliased.
    pub alias: Option<String>,
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Ord, PartialOrd, Hash, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
/// A [`Dependency`] kind.
pub enum DependencyKind {
    /// A normal dependency, comes from `dependencies:` map.
    Normal,
    /// A dev dependency, comes from `dev-dependencies:` map.
    Dev,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(rename_all = "kebab-case")]
/// Source of the dependency.
pub struct DependencySource {
    /// Python-compatibility artefact.
    pub inner: SourceInner,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(rename_all = "kebab-case", tag = "type")]
/// Actual source of the dependency.
pub enum SourceInner {
    /// A registry dependency...
    #[serde(rename_all = "kebab-case")]
    Registry {
        /// ...from this url.
        registry_url: String,
    },
    /// A git dependency.
    #[serde(rename_all = "kebab-case")]
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
#[serde(untagged, rename_all = "kebab-case")]
/// A dependency feature.
pub enum DependencyFeature {
    /// Just a feature, without conditions.
    Simple(String),
    /// Single feature + its conditions.
    Detailed(OneEntryMap<String, DependencyCondition>),
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(rename_all = "kebab-case")]
/// A single compilation profile.
pub struct Profile {
    /// Optimization level.
    pub opt_level: Option<OptLevel>,
    /// Whether should we emit DVM bytecode.
    pub dvm_bytecode: Option<bool>,
    /// Whether to use incremental compilation.
    pub incremental: Option<bool>,
    /// Whether to link C STD.
    pub c_std: Option<bool>,
    /// Whether this profile inherits other profile.
    pub inherits: Option<String>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(rename_all = "kebab-case")]
/// Duckc optimization level.
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
#[serde(rename_all = "kebab-case")]
/// Conditions of a dependency.
pub struct DependencyCondition {
    /// Required root package features.
    pub package_features: Option<Vec<String>>,
}
