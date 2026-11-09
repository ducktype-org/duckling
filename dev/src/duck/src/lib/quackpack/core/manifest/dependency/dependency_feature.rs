// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Required features of a dependency.
use std::collections::HashSet;

use super::Conditions;
use crate::quackpack::core::FeatureName;
use crate::quackpack::schemas::{OneEntryMap, registry};

#[derive(Clone, Debug)]
/// Feature of a dependency with required conditions in order to be enabled.
pub struct DependencyFeature {
    /// The feature name.
    name: FeatureName,
    /// Optional conditions for this feature to be enabled.
    conditions: Option<Conditions>,
}

impl DependencyFeature {
    /// Create a new [`DependencyFeature`].
    pub fn new(name: FeatureName, conditions: Option<Conditions>) -> Self {
        Self { name, conditions }
    }

    /// Get the feature name.
    pub fn name(&self) -> FeatureName {
        self.name
    }

    /// Check, if this feature is enabled for the given features.
    pub fn is_enabled_for(&self, enabled_features: &HashSet<FeatureName>) -> bool {
        self.conditions
            .as_ref()
            .is_none_or(|conditions| conditions.is_enabled_for(enabled_features))
    }

    /// Get the [`Conditions`] of this feature.
    pub fn conditions(&self) -> Option<&Conditions> {
        self.conditions.as_ref()
    }
}

impl From<registry::DependencyFeature> for DependencyFeature {
    fn from(value: registry::DependencyFeature) -> Self {
        match value {
            registry::DependencyFeature::Simple(name) => Self::new(name.into(), None),
            registry::DependencyFeature::Detailed(OneEntryMap {
                key: name,
                value: conditions,
            }) => Self::new(name.into(), Some(conditions.into())),
        }
    }
}

impl From<DependencyFeature> for registry::DependencyFeature {
    fn from(value: DependencyFeature) -> Self {
        let DependencyFeature { name, conditions } = value;
        match conditions {
            Some(conditions) => Self::Detailed(OneEntryMap {
                key: name.into(),
                value: conditions.into(),
            }),
            None => Self::Simple(name.into()),
        }
    }
}
