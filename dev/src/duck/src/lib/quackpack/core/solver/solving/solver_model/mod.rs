// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module containing a wrapper over [`Model`], with utilities related to dependency resolving.
//! Note:
//! -----
//! By `child` in the context of a given dependency relation we mean the package realising that dependency.
use std::collections::{HashMap, HashSet};
use std::rc::Rc;

use russcip::prelude::var;
use russcip::{Model, ProblemCreated, Solution, Variable, WithSolutions};
use tracing::{debug, trace};

use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
use crate::quackpack::core::solver::solving::input::SolverInput;
use crate::quackpack::core::solver::solving::solver_model::constraints::Constraint;
use crate::quackpack::core::{FeatureName, PackageId, Version};
use crate::{QuackResult, QuackResultContext, StrId, qp_bail};

mod constraints;

// @TODO: #3544 store information about minimal unsolvable subprogram and display it here.
#[derive(Debug)]
pub struct NoSolutionError();

impl std::fmt::Display for NoSolutionError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "no dependency resolution found")
    }
}

impl std::error::Error for NoSolutionError {}

type PresentFeature = Option<FeatureName>;

/// Creates a unique mapping of packages to their variable names.
fn package_var_name(pkg: PackageId) -> StrId {
    StrId::new(format!("{:?}@{:?}", pkg.identity(), pkg.version()))
}

/// Creates a unique mapping of pairs of form (package, feature) to their variable name.
fn package_with_feature_var_name(pkg: PackageId, feature: FeatureName) -> StrId {
    StrId::new(format!("{}@{}", package_var_name(pkg), feature))
}

/// Creates a unique mapping of a pairs of form (dependency relation, child feature) to their variable name.
fn dependency_feature_var_name(dep: DependencyEdge, feature: FeatureName) -> StrId {
    StrId::new(format!(
        "{}->{:?}@_@{}",
        package_var_name(dep.parent),
        dep.dep_identity,
        feature
    ))
}

/// Creates a unique mapping of a pairs of form (dependency relation, child version) to their variable name.
fn dependency_version_var_name(dep: DependencyEdge, version: Version) -> StrId {
    StrId::new(format!(
        "{}->{:?}@{:?}@_",
        package_var_name(dep.parent),
        dep.dep_identity,
        version
    ))
}

type FeaturesToVars = HashMap<FeatureName, Rc<Variable>>;
type ChildVersionsToVars = HashMap<Version, Rc<Variable>>;
type ChildFeaturesToVars = HashMap<FeatureName, Rc<Variable>>;

#[derive(Debug)]
/// Wrapper of [`Model`], adding mappings from appropriate variable identifiers to their variables.
pub struct SolverModel<'a, State> {
    model: Model<State>,

    input: &'a SolverInput,

    package_vars: HashMap<PackageId, Rc<Variable>>,
    package_to_feature_vars: HashMap<PackageId, FeaturesToVars>,
    dependency_to_feature_vars: HashMap<DependencyEdge, ChildFeaturesToVars>,
    dependency_to_version_vars: HashMap<DependencyEdge, ChildVersionsToVars>,

    constraints: Vec<Constraint>,
}

impl<'a> SolverModel<'a, ProblemCreated> {
    /// Creates an empty model, given packages already placed in the previous freeze and their features.
    pub fn new(input: &'a SolverInput) -> Self {
        SolverModel {
            model: Model::default().hide_output(),
            input,
            package_vars: HashMap::new(),
            package_to_feature_vars: HashMap::new(),
            dependency_to_version_vars: HashMap::new(),
            dependency_to_feature_vars: HashMap::new(),
            constraints: vec![],
        }
    }

    /// Returns the variable associated with the given pair (package, optional feature).
    /// If the feature is not supplied, returns the variable associated with the package,
    /// otherwise returns the variable associated with the pair (package, feature).
    fn get_package_variable(
        &self,
        pkg: PackageId,
        feature: PresentFeature,
    ) -> QuackResult<Rc<Variable>> {
        match feature {
            None => self
                .package_vars
                .get(&pkg)
                .cloned()
                .with_context_internal(|| {
                    format!("package variable `{pkg:?}` was not added to the model before retrieval attempt; {self:#?}")
                }),
            Some(feature) => {
                let feature_to_var_map = self.package_to_feature_vars.get(&pkg).with_context_internal(|| {
                    format!("package `{pkg:?}` and feature variable `{feature:?}` was not added to the model before retrieval attempt; {self:#?}")
                })?;
                feature_to_var_map.get(&feature).cloned().with_context_internal(|| {
                    format!("package `{pkg:?}` and feature variable `{feature:?}` was not added to the model before retrieval attempt; {self:#?}")
                })
            }
        }
    }

    /// Returns the mapping from child features to variables, associated with the given dependency.
    fn get_feature_to_var_map_for_dep(&mut self, dep: DependencyEdge) -> &ChildFeaturesToVars {
        self.dependency_to_feature_vars.entry(dep).or_default()
    }

    /// Returns the variable associated with the given (dependency, child feature) pair.
    fn get_dependency_feature_variable(
        &mut self,
        dep: DependencyEdge,
        feature: FeatureName,
    ) -> QuackResult<Rc<Variable>> {
        let feature_to_var_map = self.get_feature_to_var_map_for_dep(dep);
        feature_to_var_map.get(&feature).cloned().with_context_internal(|| format!(
            "dependency `{dep:?}` with feature variable `{feature:?}` not added to the model before retrieval of variable attempt {self:#?}")
        )
    }

    /// Returns the mapping from child versions to variables, associated with the given dependency.
    fn get_version_to_var_map_for_dep(&mut self, dep: DependencyEdge) -> &ChildVersionsToVars {
        self.dependency_to_version_vars.entry(dep).or_default()
    }

    /// Returns the variable associated with the given (dependency, child version) pair.
    fn get_dependency_version_variable(
        &mut self,
        dep: DependencyEdge,
        version: Version,
    ) -> QuackResult<Rc<Variable>> {
        let version_to_var_map = self.get_version_to_var_map_for_dep(dep);
        version_to_var_map.get(&version).cloned().with_context_internal(|| format!(
            "dependency `{dep:?}` with version {version} variable not added to the model before retrieval attempt {self:#?}")
        )
    }

    /// Creates the variable associated with the package and adds it to the model.
    pub fn add_package_var(&mut self, pkg: PackageId) {
        let objective_coef = if self.input.preexists(pkg) { 0.0 } else { 1.0 };
        let var_name = package_var_name(pkg);
        self.package_vars.entry(pkg).or_insert_with(|| {
            Rc::new(
                self.model
                    .add(var().name(&var_name).bin().obj(objective_coef)),
            )
        });
    }

    /// Creates the variable associated with the (package, feature) pair and adds it to the model.
    pub fn add_package_with_feature_var(&mut self, pkg: PackageId, feature: FeatureName) {
        let var_name = package_with_feature_var_name(pkg, feature);
        self.package_to_feature_vars
            .entry(pkg)
            .or_default()
            .entry(feature)
            .or_insert_with(|| Rc::new(self.model.add(var().name(&var_name).bin().obj(0.001))));
    }

    /// Creates a variable associated with the (dependency, child feature) pair and adds it to the model.
    pub fn add_dependency_feature_realization_var(
        &mut self,
        dep: DependencyEdge,
        feature: FeatureName,
    ) {
        let var_name = dependency_feature_var_name(dep, feature);
        self.dependency_to_feature_vars
            .entry(dep)
            .or_default()
            .entry(feature)
            .or_insert_with(|| Rc::new(self.model.add(var().name(&var_name).bin().obj(0.0))));
    }

    /// Creates a variable associated with the (dependency, child version) pair and adds it to the model.
    pub fn add_dependency_version_realization_var(
        &mut self,
        dep: DependencyEdge,
        version: Version,
    ) {
        let var_name = dependency_version_var_name(dep, version);
        self.dependency_to_version_vars
            .entry(dep)
            .or_default()
            .entry(version)
            .or_insert_with(|| Rc::new(self.model.add(var().name(&var_name).bin().obj(0.0))));
    }
}

/// Output of the solver model, contains new packages to be put into the freeze, with their features.
#[derive(Debug)]
pub struct FoundSolution {
    pub new_packages: HashSet<PackageId>,
    pub new_features: HashMap<PackageId, HashSet<FeatureName>>,
    pub new_edges: HashMap<DependencyEdge, Version>,
}

impl<'a> SolverModel<'a, ProblemCreated> {
    /// Given a constructed problem (with variables and constraints added), calls SCIP to solve it,
    /// constructs the output and returns it.
    #[tracing::instrument(target = "duck::solver", level = "debug", skip_all)]
    pub fn solve(self) -> QuackResult<FoundSolution> {
        let solve = self.model.minimize().solve();
        let Some(solution) = solve.best_sol() else {
            qp_bail!(NoSolutionError())
        };
        debug!(target: "duck::solver", ?solution);
        let preexisting_packages = self.input.all_preexisting_pkgs();
        let new_packages = new_packages(self.package_vars, &preexisting_packages, &solution);
        let new_features = new_features(
            &self.package_to_feature_vars,
            &new_packages,
            &preexisting_packages,
            self.input,
            &solution,
        );
        let new_edges = new_edges(self.dependency_to_version_vars, &solution);
        Ok(FoundSolution {
            new_packages,
            new_features,
            new_edges,
        })
    }
}

/// Helper for determining which new packages have been chosen by the model.
fn new_packages(
    package_vars: HashMap<PackageId, Rc<Variable>>,
    preexisting_packages: &HashSet<PackageId>,
    solution: &Solution,
) -> HashSet<PackageId> {
    trace!(target: "duck::solver", ?preexisting_packages, ?package_vars);
    let new_packages = package_vars
        .into_iter()
        .filter_map(|(pkg, var)| {
            if is_one(&var, solution) {
                Some(pkg)
            } else {
                None
            }
        })
        .collect::<HashSet<PackageId>>()
        .difference(preexisting_packages)
        .cloned()
        .collect::<HashSet<PackageId>>();
    trace!(target: "duck::solver", ?new_packages);
    new_packages
}

/// Helper for determining which new features of all the packages have been chosen by the model.
fn new_features(
    package_to_feature_vars: &HashMap<PackageId, FeaturesToVars>,
    new_packages: &HashSet<PackageId>,
    preexisting_packages: &HashSet<PackageId>,
    input: &SolverInput,
    solution: &Solution,
) -> HashMap<PackageId, HashSet<FeatureName>> {
    let mut new_features = HashMap::new();
    for pkg in preexisting_packages.iter().chain(new_packages.iter()) {
        let mut pkg_features = HashSet::new();
        if let Some(features_to_vars) = package_to_feature_vars.get(pkg) {
            pkg_features = features_to_vars
                .iter()
                .filter_map(|(feature, var)| {
                    if is_one(var, solution) {
                        Some(feature)
                    } else {
                        None
                    }
                })
                .cloned()
                .collect();
        }
        if let Some(features) = input.preexisting_features_for_pkg(*pkg) {
            pkg_features = pkg_features.difference(features).cloned().collect();
        }
        if !pkg_features.is_empty() {
            new_features.insert(*pkg, pkg_features);
        }
    }
    new_features
}

/// Helper for determining what new realizations of any dependencies have been chosen by the model.
fn new_edges(
    dependency_to_version_vars: HashMap<DependencyEdge, ChildVersionsToVars>,
    solution: &Solution,
) -> HashMap<DependencyEdge, Version> {
    let mut new_edges = HashMap::new();
    for (edge, version_to_var_map) in dependency_to_version_vars {
        if let Some(chosen_realization) = version_to_var_map
            .iter()
            .filter_map(|(version, var)| {
                if is_one(var, solution) {
                    Some(*version)
                } else {
                    None
                }
            })
            .next()
        {
            new_edges.insert(edge, chosen_realization);
        }
    }
    new_edges
}

/// Helper function for determining whether the model did or did not put the variable into the solution.
fn is_one(var: &Variable, solution: &Solution) -> bool {
    // The condition "> 0.5" is arbitrary, ideally it could be "== 1.0", but maybe to circumvent some float magic "> 0.5" is better.
    solution.val(var) > 0.5
}
