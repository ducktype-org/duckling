use crate::quackpack::{core::Version, schemas::OneEntryMap};
use std::collections::HashMap;

use serde::{Deserialize, Serialize};

pub type Dependencies = HashMap<String, Dependency>;

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct Manifest {
    pub metadata: Metadata,
    pub dependencies: Dependencies,
    pub dev_dependencies: Dependencies,
    pub features: HashMap<String, Vec<String>>,
    pub profiles: HashMap<String, CompilerOptions>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct Metadata {
    pub version: Version,
    pub authors: Vec<String>,
    pub license: String,
    pub name: String,
    pub description: String,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct Dependency {
    pub version: Vec<Version>,
    pub source: DependencySource,
    pub features: Vec<DependencyFeature>,
    pub pinned: bool,
    pub conditions: DependencyCondition,
    pub is_alias_for: Option<String>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct DependencySource {
    pub inner: SourceInner,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(tag = "type")]
#[serde(rename_all = "snake_case")]
pub enum SourceInner {
    Registry {
        registry_url: String,
    },
    Local {
        absolute_dir_root: String,
        dir_entry_in_manifest: String,
    },
    Git {
        git_url: String,
        commit: Option<String>,
        tag: Option<String>,
        branch: Option<String>,
    },
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
#[serde(untagged)]
pub enum DependencyFeature {
    Simple(String),
    Detailed(OneEntryMap<String, DependencyCondition>),
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct CompilerOptions {
    pub compiler_flags: Vec<String>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[cfg_attr(test, derive(Eq, PartialEq))]
pub struct DependencyCondition {
    pub package_features: Option<Vec<String>>,
}
