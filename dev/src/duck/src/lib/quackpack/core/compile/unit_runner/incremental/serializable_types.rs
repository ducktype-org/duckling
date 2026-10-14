// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! An easily serializable counterpart to [`UnitGraph`], to be used in build fingerprints.

use std::collections::{HashMap, HashSet};
use std::path::PathBuf;

use serde::{Deserialize, Serialize};

use crate::StrId;
use crate::quackpack::core::compile::unit::graph::{
    CompilationInput, GraphNodeId, UnitGraph, UnitGraphNode, UnitGraphNodeData,
};
use crate::quackpack::core::compile::unit::{PackageData, Unit, UnitType};
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::{FeatureName, Version};

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
/// A simplified, easily serializable version of [`UnitGraph`].
pub struct SerializableUnitGraph {
    pub root_id: GraphNodeId,
    pub nodes: Vec<SerializableGraphNode>,
    pub dependencies: Vec<Vec<GraphNodeId>>,
    pub packages: HashMap<Identity, SerializablePackageData>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize, Hash)]
#[serde(rename_all = "kebab-case")]
/// A simplified, easily serializable version of [`UnitGraphNode`].
pub struct SerializableGraphNode {
    pub id: GraphNodeId,
    pub node: SerializableGraphNodeData,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize, Hash)]
#[serde(rename_all = "kebab-case")]
/// A simplified, easily serializable version of [`UnitGraphNodeData`].
pub enum SerializableGraphNodeData {
    Unit(SerializableUnit),
    Input(SerializableCompilationInput),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize, Hash)]
#[serde(rename_all = "kebab-case")]
/// A simplified, easily serializable version of [`Unit`].
pub struct SerializableUnit {
    pub identity: Identity,
    pub unit_type: UnitType,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize, Hash)]
#[serde(rename_all = "kebab-case")]
/// A simplified, easily serializable version of [`CompilationInput`].
pub enum SerializableCompilationInput {
    /// Source code of a package.
    PackageSourceCode(Identity),
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
/// A simplified, easily serializable version of [`PackageData`].
pub struct SerializablePackageData {
    pub root_path: PathBuf,
    pub deps_realization: Vec<(Identity, Option<StrId>)>,
    pub enabled_features: HashSet<FeatureName>,
    pub identity: Identity,
    pub version: Version,
}

impl SerializableGraphNodeData {
    /// Get the [`Identity`] of the underlying package.
    pub fn identity(&self) -> Identity {
        match self {
            Self::Input(input) => {
                let SerializableCompilationInput::PackageSourceCode(identity) = input;
                *identity
            }
            Self::Unit(unit) => unit.identity,
        }
    }
}

impl From<&UnitGraph> for SerializableUnitGraph {
    fn from(value: &UnitGraph) -> Self {
        Self {
            root_id: value.root_id(),
            nodes: value.compilation_order().iter().map(Into::into).collect(),
            dependencies: value.all_deps().to_vec(),
            packages: value
                .all_pkgs()
                .iter()
                .map(|(k, v)| (*k, v.as_ref().into()))
                .collect(),
        }
    }
}

impl From<&UnitGraphNode> for SerializableGraphNode {
    fn from(value: &UnitGraphNode) -> Self {
        Self {
            id: value.id(),
            node: value.node().into(),
        }
    }
}

impl From<&UnitGraphNodeData> for SerializableGraphNodeData {
    fn from(value: &UnitGraphNodeData) -> Self {
        match value {
            UnitGraphNodeData::Unit(unit) => Self::Unit(unit.into()),
            UnitGraphNodeData::Input(input) => Self::Input((*input).into()),
        }
    }
}

impl From<&Unit> for SerializableUnit {
    fn from(value: &Unit) -> Self {
        Self {
            identity: value.identity(),
            unit_type: value.unit_type(),
        }
    }
}

impl From<CompilationInput> for SerializableCompilationInput {
    fn from(value: CompilationInput) -> Self {
        let CompilationInput::PackageSourceCode(identity) = value;
        Self::PackageSourceCode(identity)
    }
}

impl From<&PackageData> for SerializablePackageData {
    fn from(value: &PackageData) -> Self {
        Self {
            root_path: value.package().root().to_path_buf(),
            deps_realization: value.deps_realization().to_vec(),
            enabled_features: value.enabled_features().clone(),
            identity: value.identity(),
            version: value.package().version(),
        }
    }
}
