//! Various utilities designed to help with detecting and emitting lints.

use crate::quackpack::core::{Conditions, Dependency, DependencyFeature, Manifest};

#[derive(Debug, Clone, Copy)]
pub enum MatchedConditions<'a> {
    Dep {
        dep: &'a Dependency,
        _conds: &'a Conditions,
    },
    Feature {
        feature: &'a DependencyFeature,
        dep: &'a Dependency,
        _conds: &'a Conditions,
    },
}

/// Filter _all_ (dependencies' and their features') conditions based on a predicate.
pub fn filter_conditions<'a>(
    manifest: &'a Manifest,
    mut predicate: impl FnMut(&Conditions) -> bool,
) -> Vec<MatchedConditions<'a>> {
    let mut matched_conditions = vec![];
    for dep in manifest.dependencies().all_dependencies() {
        if let Some(conds) = dep.conditions()
            && predicate(conds)
        {
            matched_conditions.push(MatchedConditions::Dep { dep, _conds: conds });
        }
        for feature in dep.features() {
            if let Some(conds) = feature.conditions()
                && predicate(conds)
            {
                matched_conditions.push(MatchedConditions::Feature {
                    feature,
                    dep,
                    _conds: conds,
                });
            }
        }
    }
    matched_conditions
}
