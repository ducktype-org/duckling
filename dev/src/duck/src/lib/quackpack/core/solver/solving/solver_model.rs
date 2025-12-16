use std::collections::HashMap;

use russcip::{Model, ProblemCreated, Variable, prelude::var};

use crate::{
    StrId,
    quackpack::core::{
        FeatureName, Version,
        solver::types::{ExpandedPackage, ParentWithDependencyLoc},
    },
};

fn package_var_name(pkg: &ExpandedPackage) -> StrId {
    StrId::new(format!("{:?}@{:?}", pkg.location(), pkg.version()))
}

fn package_with_feature_var_name(pkg: &ExpandedPackage, feature: FeatureName) -> StrId {
    StrId::new(format!("{}@{:?}", package_var_name(pkg), feature))
}

fn dependency_feature_var_name(dep: &ParentWithDependencyLoc, feature: FeatureName) -> StrId {
    StrId::new(format!(
        "{}->{:?}@_@{:?}",
        package_var_name(&dep.parent),
        dep.dependency_loc,
        feature
    ))
}

fn dependency_version_var_name(dep: &ParentWithDependencyLoc, version: Option<Version>) -> StrId {
    StrId::new(format!(
        "{}->{:?}@{:?}@_",
        package_var_name(&dep.parent),
        dep.dependency_loc,
        version
    ))
}

type FeaturesToVars = HashMap<FeatureName, Variable>;
type ChildVersionsToVars = HashMap<Option<Version>, Variable>;
type ChildFeaturesToVars = HashMap<FeatureName, Variable>;
pub struct SolverModel<State> {
    model: Model<State>,
    package_vars: HashMap<ExpandedPackage, Variable>,
    package_to_feature_vars: HashMap<ExpandedPackage, FeaturesToVars>,
    dependency_to_version_vars: HashMap<ParentWithDependencyLoc, ChildVersionsToVars>,
    dependency_to_feature_vars: HashMap<ParentWithDependencyLoc, ChildFeaturesToVars>,
}

impl SolverModel<ProblemCreated> {
    pub fn new() -> Self {
        SolverModel {
            model: Model::default().hide_output(),
            package_vars: HashMap::new(),
            package_to_feature_vars: HashMap::new(),
            dependency_to_version_vars: HashMap::new(),
            dependency_to_feature_vars: HashMap::new(),
        }
    }
}

// Functions for adding varaibles.
impl SolverModel<ProblemCreated> {
    pub fn add_package_var(&mut self, pkg: ExpandedPackage) {
        let var_name = package_var_name(&pkg);
        self.package_vars
            .entry(pkg)
            .or_insert_with(|| self.model.add(var().name(&var_name).bin()));
    }

    pub fn add_package_with_feature_var(&mut self, pkg: ExpandedPackage, feature: FeatureName) {
        let var_name = package_with_feature_var_name(&pkg, feature);
        self.package_to_feature_vars
            .entry(pkg)
            .or_insert(FeaturesToVars::new())
            .entry(feature)
            .or_insert_with(|| self.model.add(var().name(&var_name).bin()));
    }

    pub fn add_dependency_feature_realisation_var(
        &mut self,
        dep: ParentWithDependencyLoc,
        feature: FeatureName,
    ) {
        let var_name = dependency_feature_var_name(&dep, feature);
        self.dependency_to_feature_vars
            .entry(dep)
            .or_insert(ChildFeaturesToVars::new())
            .entry(feature)
            .or_insert_with(|| self.model.add(var().name(&var_name).bin()));
    }

    pub fn add_dependency_version_realisation_var(
        &mut self,
        dep: ParentWithDependencyLoc,
        version: Option<Version>,
    ) {
        let var_name = dependency_version_var_name(&dep, version);
        self.dependency_to_version_vars
            .entry(dep)
            .or_insert(ChildVersionsToVars::new())
            .entry(version)
            .or_insert_with(|| self.model.add(var().name(&var_name).bin()));
    }
}
