// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
    //  Connected with @TODO: #3384 in `are_features_enabled`.
    pub fn is_enabled_for(&self, enabled_features: &HashSet<FeatureName>) -> bool {
        self.are_features_enabled(enabled_features)
    }

    /// Check, if enabled features for this package enable this dependency.
    fn are_features_enabled(&self, enabled_features: &HashSet<FeatureName>) -> bool {
        let Some(ref features) = self.required_root_package_features else {
            return true;
        };
        let required_features = HashSet::from_iter(features.iter().copied());
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

    #[test]
    // @TODO: #3384 When we begin to check host system, add also that.
    //  We will probably need to do some conditional logic (make sure it runs on CI!).
    fn enabled_conditions() {
        let empty_condition = Conditions::new(None);
        assert!(empty_condition.is_enabled_for(&[].into()));
        assert!(empty_condition.is_enabled_for(&["a".into()].into()));

        let a_b_condition = Conditions::new(Some(["a".into(), "b".into()].into()));

        assert!(a_b_condition.is_enabled_for(&["a".into(), "c".into()].into()));

        assert!(!a_b_condition.is_enabled_for(&["c".into(), "d".into()].into()));

        assert!(a_b_condition.is_enabled_for(&["a".into(), "b".into(), "c".into()].into()));

        let a_condition = Conditions::new(Some(["a".into()].into()));

        assert!(!a_condition.is_enabled_for(&[].into()));

        assert!(a_condition.is_enabled_for(&["a".into()].into()));

        assert!(!a_condition.is_enabled_for(&["b".into()].into()));
    }
}
