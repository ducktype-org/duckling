//! Module containing a wrapper over [`Model`], with utilities related to dependency resolving.
//! Note:
//! -----
//! By `child` in the context of a given dependency relation we mean the package realising that dependency.
use std::collections::{HashMap, HashSet};
use std::rc::Rc;

use russcip::prelude::{cons, var};
use russcip::{Model, ProblemCreated, Solution, Variable, WithSolutions};
use tracing::debug;

use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
use crate::quackpack::core::solver::solving::scip_ext::BinModelExt;
use crate::quackpack::core::solver::solving::solver_engine::SolverInput;
use crate::quackpack::core::{FeatureName, PackageId, Version};
use crate::{QuackResult, QuackResultContext, StrId};

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

    /// Adds a constraint that forces the package to be present in the solution to the model.
    pub fn require_package(&mut self, pkg: PackageId) -> QuackResult<()> {
        let var = self.get_package_variable(pkg, None)?;
        self.model.add(cons().coef(&var, 1.0).eq(1.0));
        Ok(())
    }

    /// Adds a constraint that forces the package to be present with a feature.
    pub fn require_package_with_feature(
        &mut self,
        pkg: PackageId,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var = self.get_package_variable(pkg, Some(feature))?;
        self.model.add(cons().coef(&var, 1.0).eq(1.0));
        Ok(())
    }

    /// Add a constraint that this package can never be present.
    pub fn forbid_package(&mut self, pkg: PackageId) -> QuackResult<()> {
        let var = self.get_package_variable(pkg, None)?;
        self.model.add(cons().coef(&var, 1.0).eq(0.0));
        Ok(())
    }

    /// Add a constraint that this package can never be present with the given feature.
    pub fn forbid_package_with_feature(
        &mut self,
        pkg: PackageId,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var = self.get_package_variable(pkg, Some(feature))?;
        self.model.add(cons().coef(&var, 1.0).eq(0.0));
        Ok(())
    }

    /// Adds a constraint that forces the child to be present with all of the required features.
    pub fn require_satisfying_dep_feature(
        &mut self,
        edge: DependencyEdge,
        parent_feature: PresentFeature,
        child_features: &HashSet<FeatureName>,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(edge.parent, parent_feature)?;
        let feature_vars = child_features
            .iter()
            .map(|feature| self.get_dependency_feature_variable(edge, *feature))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        self.model.one_implies_all(parent_var, feature_vars);
        Ok(())
    }

    /// For each pair (parent_feature, forced_child_features) adds a condition that
    /// presence of the parent package with parent feature forces presence of the child with all the forced features.
    /// Important:
    /// ----------
    /// Use only for scenarios where both parent and child have belonged to the previous freeze.
    pub fn require_satisfying_dep_feature_for_preexisting(
        &mut self,
        parent: PackageId,
        child: PackageId,
        forcing: Vec<(FeatureName, Vec<FeatureName>)>,
    ) -> QuackResult<()> {
        for (parent_feature, forced_child_features) in forcing {
            let parent_var = self.get_package_variable(parent, Some(parent_feature))?;
            let child_vars = forced_child_features
                .into_iter()
                .map(|feature| self.get_package_variable(child, Some(feature)))
                .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
            self.model.one_implies_all(parent_var, child_vars);
        }
        Ok(())
    }

    /// Adds a constraint that forces the child to be present in at least one of the required versions.
    pub fn require_satisfying_dep_version(
        &mut self,
        edge: DependencyEdge,
        parent_feature: PresentFeature,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(edge.parent, parent_feature)?;
        let version_vars = self
            .get_version_to_var_map_for_dep(edge)
            .values()
            .cloned()
            .collect();
        self.model.all_implies_any(vec![parent_var], version_vars);
        Ok(())
    }

    /// Adds a constraint that if the variable associated with a pair (dependency, child version)
    /// was chosen by the model to be present, the underlying package variable of the child in that version
    /// has to be present.
    pub fn require_substantiate_dep(&mut self, edge: DependencyEdge) -> QuackResult<()> {
        for (pkg_version, version_realization_var) in
            self.get_version_to_var_map_for_dep(edge).clone()
        {
            let pkg_var =
                self.get_package_variable(PackageId::new(edge.dep_identity, pkg_version), None)?;
            self.model.implies(&version_realization_var, &pkg_var);
        }
        Ok(())
    }

    /// Adds a constraint that if the variable associated with a pair (dependency, child version) was chosen to be present
    /// and the variable associated with a pair (dependency, child's feature) was chosen to be present,
    /// then the underlying package variable of the child in that version with that feature has to be present.
    pub fn require_substantiate_dep_features(
        &mut self,
        dep: DependencyEdge,
        solver_input: &SolverInput,
        possible_dep_realizations: &[PackageId],
    ) -> QuackResult<()> {
        for pkg in possible_dep_realizations {
            let version_realization_var =
                self.get_dependency_version_variable(dep, pkg.version())?;
            for (feature, feature_realization_var) in
                self.get_feature_to_var_map_for_dep(dep).clone()
            {
                let pkg_features = solver_input.package_data(*pkg)?.features();
                if !pkg_features.contains(&feature) {
                    self.model.all_implies_any(
                        vec![
                            version_realization_var.clone(),
                            feature_realization_var.clone(),
                        ],
                        vec![],
                    );
                } else {
                    self.model.all_implies_any(
                        vec![
                            version_realization_var.clone(),
                            feature_realization_var.clone(),
                        ],
                        vec![self.get_package_variable(*pkg, Some(feature))?],
                    );
                }
            }
        }
        Ok(())
    }

    /// Adds a constraint the pair (pkg, feature) implies the presence of pairs (pkg, F) for any feature F to which `feature` expands.
    pub fn require_features_expansion(
        &mut self,
        pkg: PackageId,
        feature: FeatureName,
        expanded_features: impl IntoIterator<Item = FeatureName>,
    ) -> QuackResult<()> {
        let expanded_features_vars: QuackResult<Vec<Rc<Variable>>> = expanded_features
            .into_iter()
            .map(|f| self.get_package_variable(pkg, Some(f)))
            .collect();
        let expanded_features_vars = expanded_features_vars?;
        let forcing_feature_var = self.get_package_variable(pkg, Some(feature))?;
        self.model
            .one_implies_all(forcing_feature_var, expanded_features_vars);
        Ok(())
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
    #[tracing::instrument(skip_all)]
    pub fn solve(self) -> QuackResult<FoundSolution> {
        let solve = self.model.minimize().solve();
        let solution = solve.best_sol().context("Failed to find a solution")?;
        debug!(?solution);
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
#[tracing::instrument(skip_all)]
fn new_packages(
    package_vars: HashMap<PackageId, Rc<Variable>>,
    preexisting_packages: &HashSet<PackageId>,
    solution: &Solution,
) -> HashSet<PackageId> {
    debug!(?preexisting_packages, ?package_vars);
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
    debug!(?new_packages);
    new_packages
}

/// Helper for determining which new features of all the packages have been chosen by the model.
#[tracing::instrument(skip_all)]
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
#[tracing::instrument(skip_all)]
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
