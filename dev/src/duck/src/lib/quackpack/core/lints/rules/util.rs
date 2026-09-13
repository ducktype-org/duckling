//! Various utilities designed to help with detecting and emitting lints.

use crate::quackpack::core::{Conditions, Dependency, DependencyFeature, FeatureName, Manifest};

#[derive(Debug, Clone, Copy)]
/// An input to [`walk_conditions`].
/// Indicates what type of [`Conditions`] is matched.
pub enum MatchedConditions<'a> {
    Dep {
        dep: &'a Dependency,
        conds: &'a Conditions,
    },
    Feature {
        feature: &'a DependencyFeature,
        dep: &'a Dependency,
        conds: &'a Conditions,
    },
}

impl<'a> MatchedConditions<'a> {
    /// Get the [`Dependency`].
    #[expect(dead_code)]
    pub fn dep(self) -> &'a Dependency {
        match self {
            Self::Dep { dep, conds: _ } => dep,
            Self::Feature {
                feature: _,
                dep,
                conds: _,
            } => dep,
        }
    }

    /// Get the [`Conditions`].
    pub fn conds(self) -> &'a Conditions {
        match self {
            Self::Dep { dep: _, conds } => conds,
            Self::Feature {
                feature: _,
                dep: _,
                conds,
            } => conds,
        }
    }

    /// Maybe get the [`DependencyFeature`].
    #[expect(dead_code)]
    pub fn maybe_dep_feature(self) -> Option<&'a DependencyFeature> {
        match self {
            Self::Dep { dep: _, conds: _ } => None,
            Self::Feature {
                feature,
                dep: _,
                conds: _,
            } => Some(feature),
        }
    }
}

/// Filter _all_ (dependencies' and their features') conditions based on a predicate.
pub fn walk_conditions<'a>(
    manifest: &'a Manifest,
    mut predicate: impl FnMut(MatchedConditions<'a>),
) {
    for dep in manifest.dependencies().all_dependencies() {
        if let Some(conds) = dep.conditions() {
            predicate(MatchedConditions::Dep { dep, conds });
        }
        for feature in dep.features() {
            if let Some(conds) = feature.conditions() {
                predicate(MatchedConditions::Feature {
                    feature,
                    dep,
                    conds,
                });
            }
        }
    }
}

/// Filter the features, returning only the nonexistent ones.
pub fn nonexistent_features<'a>(
    manifest: &'a Manifest,
    required_features: &'a [FeatureName],
) -> impl Iterator<Item = FeatureName> + 'a {
    required_features
        .iter()
        .copied()
        .filter(|feature| is_nonexistent_feature(manifest, *feature))
}

/// Check whether a feature is nonexistent.
pub fn is_nonexistent_feature(manifest: &Manifest, feature: FeatureName) -> bool {
    !manifest.features().has_feature(feature)
}
