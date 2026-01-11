use std::{
    collections::{HashMap, HashSet},
    iter::once,
};

use russcip::ProblemCreated;

use crate::{
    QuackResult, QuackResultContext, StrId,
    quackpack::core::{
        Dependency, FeatureName, Manifest, Version,
        solving::solver_model::{FoundSolution, SolverModel},
        types_common::{DependencyEdge, ExpandedLocation, ExpandedPackage, Location},
        util::get_possible_realisations,
    },
};

pub struct GatheredInfo {
    pub gathered_manifests: HashMap<ExpandedPackage, Manifest>,
    pub all_possible_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
    pub versions_for_location: HashMap<ExpandedLocation, Vec<Option<Version>>>,
    pub location_resolver: HashMap<Location, ExpandedLocation>,

    pub preexisting_packages: HashSet<ExpandedPackage>,
    pub preexisting_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
}

pub fn run_engine(
    input: &GatheredInfo,
    new_dependencies: &Vec<(ExpandedPackage, HashSet<FeatureName>)>,
) -> QuackResult<FoundSolution> {
    let engine = SolverEngine::new(input);
    engine.run(new_dependencies)
}

pub struct SolverEngine<'a> {
    input: &'a GatheredInfo,
    model: SolverModel<'a, ProblemCreated>,
}

impl<'a> SolverEngine<'a> {
    fn new(input: &'a GatheredInfo) -> Self {
        Self {
            input,
            model: SolverModel::new(&input.preexisting_packages, &input.preexisting_features),
        }
    }

    fn run(
        mut self,
        new_dependencies: &Vec<(ExpandedPackage, HashSet<FeatureName>)>,
    ) -> QuackResult<FoundSolution> {
        self.create_package_variables()?;
        let empty_hashset: HashSet<FeatureName> = HashSet::new();
        for (package, manifest) in self.input.gathered_manifests.iter() {
            let possible_features = self
                .input
                .all_possible_features
                .get(package)
                .unwrap_or_else(|| &empty_hashset);
            for dependency in manifest.dependencies().all_dependencies().values() {
                if dependency.is_enabled_for(possible_features.iter().cloned()) {
                    self.construct_for_single_dependency(package, dependency)?;
                }
            }
        }

        for (new_dep, new_dep_features) in new_dependencies.iter() {
            self.model.require_package(new_dep)?;
            for feature in new_dep_features.iter() {
                self.model.require_package_with_feature(new_dep, *feature)?;
            }
        }
        Ok(self.model.solve())
    }

    fn create_package_variables(&mut self) -> QuackResult<()> {
        for pkg in self.input.gathered_manifests.keys() {
            self.model.add_package_var(pkg.clone())?;
            for possible_features in self.input.all_possible_features.get(pkg).iter() {
                for feature in possible_features.iter() {
                    self.model
                        .add_package_with_feature_var(pkg.clone(), feature.clone())?;
                }
            }
        }
        Ok(())
    }

    fn construct_for_single_dependency(
        &mut self,
        parent: &ExpandedPackage,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let possible_realizations = get_possible_realisations(
            manifest_dependency,
            &self.input.versions_for_location,
            &self.input.location_resolver,
        )?;

        let edge = DependencyEdge::from_manifest_and_parent(
            parent.clone(),
            manifest_dependency,
            &self.input.location_resolver,
        )
        .context_internal("Failed to expand a location")?;

        self.create_dependency_version_realization_conditions(
            &edge,
            manifest_dependency,
            &possible_realizations,
        )?;
        self.create_dependency_feature_realization_conditions(&edge, manifest_dependency)?;
        self.model.require_substantiate_dep(&edge)?;
        self.model.require_substantiate_dep_features(
            &edge,
            &self.input.all_possible_features,
            possible_realizations,
        )?;
        Ok(())
    }

    fn create_dependency_version_realization_conditions(
        &mut self,
        edge: &DependencyEdge,
        manifest_dependency: &Dependency,
        possible_realizations: &Vec<ExpandedPackage>,
    ) -> QuackResult<()> {
        for realization in possible_realizations.iter() {
            self.model.add_dependency_version_realisation_var(
                edge.clone(),
                realization.version.clone(),
            )?;
        }

        let is_dep_forced_default = manifest_dependency.is_enabled_for(vec![]);
        if is_dep_forced_default {
            self.model.require_satisfying_dep_version(edge, None)?;
        } else {
            for dep_forcing_feature in manifest_dependency.enableing_features() {
                self.model
                    .require_satisfying_dep_version(edge, Some(dep_forcing_feature))?;
            }
        }
        Ok(())
    }

    fn create_dependency_feature_realization_conditions(
        &mut self,
        edge: &DependencyEdge,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let parent_features = parent_features_to_consider(self.input, edge);

        let enabled_always = HashSet::from_iter(manifest_dependency.enabled_features(vec![]));
        let mut tmp_hash_set;
        for parent_feature in parent_features {
            let forced = match parent_feature {
                None => &enabled_always,
                Some(feature) => {
                    tmp_hash_set =
                        HashSet::from_iter(manifest_dependency.enabled_features(vec![*feature]))
                            .difference(&enabled_always)
                            .cloned()
                            .collect();
                    &tmp_hash_set
                }
            };
            if forced.is_empty() {
                continue;
            }
            for feature in forced.iter() {
                self.model
                    .add_dependency_feature_realisation_var(edge.clone(), *feature)?;
            }
            self.model
                .require_satisfying_dep_feature(edge, parent_feature.cloned(), forced)?;
        }
        Ok(())
    }
}

fn parent_features_to_consider<'a>(
    input: &'a GatheredInfo,
    edge: &DependencyEdge,
) -> impl Iterator<Item = Option<&'a StrId>> {
    input
        .all_possible_features
        .get(&edge.parent)
        .into_iter()
        .flatten()
        .map(|feature| Some(feature))
        .chain(once(None))
}
