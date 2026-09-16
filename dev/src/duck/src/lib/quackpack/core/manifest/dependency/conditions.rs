//! Conditions describing whether a dependency should be enabled.
use std::collections::HashSet;

use crate::quackpack::core::FeatureName;
use crate::quackpack::schemas::registry;

#[derive(Clone, Debug)]
/// Conditions required by a dependency or a feature flag in order to be enabled.
/// This is enabled for `any(system) and any(arch) and any(flags)`.
pub struct Conditions {
    /// Required root package features for this condition.
    required_root_package_features: Option<Vec<FeatureName>>,
}

impl Conditions {
    /// Create new conditions with validation.
    ///
    /// Fails if any of the optional vectors are present but empty.
    pub fn new(required_root_package_features: Option<Vec<FeatureName>>) -> Self {
        Self {
            required_root_package_features,
        }
    }

    /// Check, if conditions are met for the given enabled features.
    /// This checks `any(system) and any(arch) and any(flags)`.
    // @TODO: #2705 Do we want to take an `impl IntoIterator`, or a `Vec`, or a `HashSet`?
    //  Connected with @TODO: #3384 in `are_features_enabled`.
    pub fn is_enabled_for(&self, enabled_features: impl IntoIterator<Item = FeatureName>) -> bool {
        self.are_features_enabled(enabled_features)
    }

    /// Check, if enabled features for this package enable this dependency.
    fn are_features_enabled(
        &self,
        enabled_features: impl IntoIterator<Item = FeatureName>,
    ) -> bool {
        let Some(ref features) = self.required_root_package_features else {
            return true;
        };
        let enabled_features = enabled_features.into_iter().collect::<HashSet<_>>();
        let required_features = HashSet::from_iter(features.iter().copied());
        // NOTE: Leaving this as-is, because it's easier to parse.
        // @TODO: #2705 We could work with plain iterators and/or keep `required_root_package_features` as a HashSet,
        //  but only if creating temporary HashSets becomes a bottleneck. Also connected with !TODO above, in `is_enabled_for`.
        !enabled_features.is_disjoint(&required_features)
    }

    /// Returns root package's features mentioned in the manifest
    pub fn required_root_package_features(&self) -> Option<&[FeatureName]> {
        self.required_root_package_features.as_deref()
    }
}

impl From<registry::DependencyCondition> for Conditions {
    fn from(value: registry::DependencyCondition) -> Self {
        let features = value.package_features;
        let features = features.map(|vec| vec.into_iter().map(Into::into).collect());
        Self::new(features)
    }
}

impl From<Conditions> for registry::DependencyCondition {
    fn from(value: Conditions) -> Self {
        let features = value.required_root_package_features;
        let features = features.map(|vec| vec.into_iter().map(Into::into).collect());
        Self {
            package_features: features,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::StrId;

    #[test]
    // @TODO: #3384 When we begin to check host system, add also that.
    //  We will probably need to do some conditional logic (make sure it runs on CI!).
    fn enabled_conditions() {
        let empty_condition = Conditions::new(None);
        assert!(empty_condition.is_enabled_for(vec![]));
        assert!(empty_condition.is_enabled_for(vec![StrId::new("a")]));

        let a_b_condition = Conditions::new(Some(vec![StrId::new("a"), StrId::new("b")]));

        assert!(a_b_condition.is_enabled_for(vec![StrId::new("a"), StrId::new("c")]));

        assert!(!a_b_condition.is_enabled_for(vec![StrId::new("c"), StrId::new("d")]));

        assert!(a_b_condition.is_enabled_for(vec![
            StrId::new("a"),
            StrId::new("b"),
            StrId::new("c"),
        ]));

        let a_condition = Conditions::new(Some(vec![StrId::new("a")]));

        assert!(!a_condition.is_enabled_for(vec![]));

        assert!(a_condition.is_enabled_for(vec![StrId::new("a")]));

        assert!(!a_condition.is_enabled_for(vec![StrId::new("b")]));
    }
}
