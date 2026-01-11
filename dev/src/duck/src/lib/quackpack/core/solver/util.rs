use std::collections::HashMap;

use crate::{
    QuackResult, QuackResultContext,
    quackpack::core::{
        Dependency, Version,
        types_common::{ExpandedLocation, ExpandedPackage, Location, Package},
        version::CompatibilityCheck,
    },
};

pub fn get_possible_realisations(
    dependency_description: &Dependency,
    versions_for_location: &HashMap<ExpandedLocation, Vec<Option<Version>>>,
    location_resolver: &HashMap<Location, ExpandedLocation>,
) -> QuackResult<Vec<ExpandedPackage>> {
    if dependency_description.is_pinned() {
        let version = dependency_description
            .desc()
            .versions()
            .get(0)
            .context_internal("Pinned dependency should have exactly one version specified")?;
        let only_package = Package {
            location: Location::from(dependency_description),
            version: Some(version.clone()),
        }
        .resolve(location_resolver);
        Ok(only_package.iter().cloned().collect())
    } else {
        let Some(location) = location_resolver.get(&Location::from(dependency_description)) else {
            return Ok(vec![]);
        };
        let baseline_versions = if location.is_local() {
            vec![None]
        } else {
            dependency_description
                .desc()
                .versions()
                .iter()
                .copied()
                .map(|v| Some(v))
                .collect()
        };
        let Some(possible_versions) = versions_for_location.get(&location) else {
            return Ok(vec![]);
        };
        let good_versions: Vec<Option<Version>> = possible_versions
            .iter()
            .filter(|version| {
                baseline_versions
                    .iter()
                    .any(|baseline| baseline.can_be_upgraded_to(*version))
            })
            .copied()
            .collect();
        Ok(good_versions
            .into_iter()
            .map(|version| ExpandedPackage {
                location: location.clone(),
                version,
            })
            .collect())
    }
}
