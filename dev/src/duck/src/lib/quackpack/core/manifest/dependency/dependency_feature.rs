use super::Conditions;

use crate::quackpack::core::FeatureName;

#[derive(Debug)]
/// Feature of a dependency with required conditions in order to be enabled.
pub struct DependencyFeature {
    /// The feature name.
    name: FeatureName,
    /// Optional conditions for this feature to be enabled.
    conditions: Option<Conditions>,
}

impl DependencyFeature {
    /// Create a new dependency feature.
    pub fn new(name: FeatureName, conditions: Option<Conditions>) -> Self {
        Self { name, conditions }
    }

    /// Get the feature name.
    pub fn name(&self) -> FeatureName {
        self.name
    }

    /// Check if this feature is enabled for the given features.
    pub fn is_enabled_for(&self, enabled_features: impl IntoIterator<Item = FeatureName>) -> bool {
        self.conditions
            .as_ref()
            .is_none_or(|conditions| conditions.is_enabled_for(enabled_features))
    }
}
