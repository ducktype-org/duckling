use std::collections::hash_map::Entry;
use std::collections::{HashMap, HashSet};

use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::gathering::gatherer_state::{self, GatheredInfo};
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::{FeatureName, Manifest, PackageId, Source, Version};
use crate::{QuackResult, QuackResultContext, StrId};

/// Struct with all the necessary information for the solver to be run.
#[derive(Clone, Debug)]
pub(in crate::quackpack::core::solver) struct SolverInput {
    pub packages_data: HashMap<PackageId, PackageData>,
    pub versions_for_identity: HashMap<FullIdentity, HashSet<Version>>,
    pub source_to_origin_resolver: HashMap<Source, FullOrigin>,
}

/// All necessacry information for a singular package.
#[derive(Clone, Debug)]
pub(in crate::quackpack::core::solver) struct PackageData {
    /// Manifest of the package.
    manifest: Box<Manifest>,
    /// All features which can potentially occur in the solution.
    features: HashSet<FeatureName>,
    /// Whether and how the package was present in the previous freeze.
    preexistance: Option<PreexistanceData>,
}

/// Information about a package in the previous freeze.
#[derive(Clone, Debug, Default)]
pub(in crate::quackpack::core::solver) struct PreexistanceData {
    features: HashSet<FeatureName>,
    realized_dependencies: HashMap<StrId, PackageId>,
}

impl SolverInput {
    /// Creates the [`SolverInput`], based on the previous freeze, its packages' manifests
    /// and information gathered in the gathering phase.
    #[tracing::instrument(skip_all)]
    pub fn from_freeze_and_gathered_info(
        prev_freeze: &SolverFreeze,
        mut prev_freeze_manifests: HashMap<PackageId, Box<Manifest>>,
        gathered_info: GatheredInfo,
    ) -> QuackResult<Self> {
        let mut packages_data: HashMap<PackageId, PackageData> = gathered_info
            .packages_data
            .into_iter()
            .map(|(k, v)| (k, v.into()))
            .collect();
        let mut versions_for_identity = gathered_info.versions_for_identity;
        let mut source_to_origin_resolver = gathered_info.source_to_origin_resolver;
        for (pkg, freeze) in prev_freeze.package_freezes.iter() {
            let manifest = prev_freeze_manifests
                .remove(pkg)
                .with_context_internal(|| {
                    format!("previous freeze package {pkg:?} without fetched manifest")
                })?;
            let pkg_data = match packages_data.entry(*pkg) {
                Entry::Occupied(data) => {
                    // Remember that for gathering we modified the root packages manifest.
                    // To revert that change now, we overwrite manifests from gathering with manifests from diagnosing previous freeze.
                    let data = data.into_mut();
                    data.manifest = manifest;
                    data
                }
                Entry::Vacant(vacant) => vacant.insert(PackageData::new_empty(manifest)),
            };
            pkg_data.features.extend(freeze.features.iter().copied());
            let mut preexistance_data = PreexistanceData::default();
            preexistance_data.features = freeze.features.clone();
            preexistance_data.realized_dependencies = freeze.dependencies_realization.clone();
            pkg_data.preexistance = Some(preexistance_data);
            versions_for_identity
                .entry(pkg.identity())
                .or_default()
                .insert(pkg.version());
            source_to_origin_resolver.insert(
                Source::canonical_source_for_origin(pkg.origin()),
                pkg.origin(),
            );
        }
        Ok(Self {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        })
    }

    /// Transform [`SolverInput`] into a mapping from packages to manifests.
    pub fn into_manifests(self) -> HashMap<PackageId, Box<Manifest>> {
        self.packages_data
            .into_iter()
            .map(|(pkg, data)| (pkg, data.manifest))
            .collect()
    }

    /// Check if package existed in the previous freeze.
    pub fn preexists(&self, pkg: PackageId) -> bool {
        self.packages_data
            .get(&pkg)
            .is_some_and(|data| data.preexistance.is_some())
    }

    /// Get a [`HashSet`] of preexisting features if the package preexisted in the previous freeze.
    pub fn preexisting_features_for_pkg(&self, pkg: PackageId) -> Option<&HashSet<FeatureName>> {
        self.packages_data
            .get(&pkg)
            .and_then(|data| data.preexistance.as_ref().map(|pre| &pre.features))
    }

    /// Get a [`HashSet`] of all packages preexisting in the previous freeze.
    pub fn all_preexisting_pkgs(&self) -> HashSet<PackageId> {
        self.packages_data
            .iter()
            .filter_map(|(pkg, data)| {
                if data.preexistance.is_some() {
                    Some(*pkg)
                } else {
                    None
                }
            })
            .collect()
    }

    /// Get the [`PackageData`] associated with the package.
    /// Important:
    /// ----------
    /// Returns internal error if no data is found.
    pub fn package_data(&self, pkg: PackageId) -> QuackResult<&PackageData> {
        self.packages_data.get(&pkg).with_context_internal(|| {
            format!("package {pkg:?} not present in packages to package data map")
        })
    }
}

impl PackageData {
    /// Create a [`PackageData`] without features and preexistance info.
    /// Should be used as an intermediate step when constructing [`PackageData`] instances.
    pub fn new_empty(manifest: Box<Manifest>) -> PackageData {
        Self {
            manifest,
            features: [].into(),
            preexistance: None,
        }
    }

    pub fn manifest(&self) -> &Manifest {
        &self.manifest
    }

    /// Get the [`HashSet`] which could occur in the solution.
    pub fn features(&self) -> &HashSet<FeatureName> {
        &self.features
    }

    /// Check if a given feature was present in the previous freeze.
    pub fn feature_preexists(&self, feature: FeatureName) -> bool {
        self.preexistance
            .as_ref()
            .is_some_and(|data| data.features.contains(&feature))
    }

    /// Check whether a given dependency was present in the previous freeze and get the realization.
    pub fn dependency_preexists(&self, name: StrId) -> Option<PackageId> {
        self.preexistance
            .as_ref()?
            .realized_dependencies
            .get(&name)
            .copied()
    }

    #[cfg(test)]
    pub fn new(
        manifest: Box<Manifest>,
        features: HashSet<FeatureName>,
        preexistance: Option<PreexistanceData>,
    ) -> Self {
        Self {
            manifest,
            features,
            preexistance,
        }
    }
}

impl From<gatherer_state::PackageData> for PackageData {
    fn from(value: gatherer_state::PackageData) -> Self {
        Self {
            manifest: value.manifest,
            features: value.requested_features,
            preexistance: None,
        }
    }
}

#[cfg(test)]
impl PreexistanceData {
    pub fn new(features: HashSet<FeatureName>, depedencies: HashMap<StrId, PackageId>) -> Self {
        Self {
            features,
            realized_dependencies: depedencies,
        }
    }
}
