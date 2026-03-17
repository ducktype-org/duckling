//! Required features of a dependency.
use super::Conditions;

use crate::QuackError;
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
    pub fn is_enabled_for(&self, enabled_features: impl IntoIterator<Item = FeatureName>) -> bool {
        self.conditions
            .as_ref()
            .is_none_or(|conditions| conditions.is_enabled_for(enabled_features))
    }
}

impl TryFrom<registry::DependencyFeature> for DependencyFeature {
    type Error = QuackError;

    fn try_from(value: registry::DependencyFeature) -> Result<Self, Self::Error> {
        match value {
            registry::DependencyFeature::Simple(name) => Ok(Self::new(name.into(), None)),
            registry::DependencyFeature::Detailed(OneEntryMap {
                key: name,
                value: conditions,
            }) => Ok(Self::new(name.into(), Some(conditions.try_into()?))),
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
