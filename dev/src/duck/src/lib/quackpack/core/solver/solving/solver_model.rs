/// Module containing a wrapper over russcip::Model, with utilities related to dependency resolving.
/// By `child` in the context of a given dependency relation we mean the package realising that dependency.
use std::{
    collections::{HashMap, HashSet},
    rc::Rc,
};

use russcip::{
    Model, ProblemCreated, Variable,
    prelude::{cons, var},
};

use crate::{
    QuackResult, QuackResultContext, StrId,
    quackpack::core::{
        FeatureName, Version,
        solver::{
            solving::scip_ext::BinModelExt,
            types_common::{DependencyEdge, ExpandedPackage, PresentFeature},
        },
    },
};

/// Creates a unique mapping of packages to their variable names.
fn package_var_name(pkg: &ExpandedPackage) -> StrId {
    StrId::new(format!("{:?}@{:?}", pkg.location(), pkg.version()))
}

/// Creates a unique mapping of pairs of form (package, feature) to its variable name.
fn package_with_feature_var_name(pkg: &ExpandedPackage, feature: FeatureName) -> StrId {
    StrId::new(format!("{}@{:?}", package_var_name(pkg), feature))
}

/// Creates a unique mapping of a pair of form (dependency relation, child feature) to its variable name.
fn dependency_feature_var_name(dep: &DependencyEdge, feature: FeatureName) -> StrId {
    StrId::new(format!(
        "{}->{:?}@_@{:?}",
        package_var_name(&dep.parent),
        dep.dependency_loc,
        feature
    ))
}

/// Creates a unique mapping of a pair of form (dependency relation, child version) to its variable name.
fn dependency_version_var_name(dep: &DependencyEdge, version: Option<Version>) -> StrId {
    StrId::new(format!(
        "{}->{:?}@{:?}@_",
        package_var_name(&dep.parent),
        dep.dependency_loc,
        version
    ))
}

type FeaturesToVars = HashMap<FeatureName, Rc<Variable>>;
type ChildVersionsToVars = HashMap<Option<Version>, Rc<Variable>>;
type ChildFeaturesToVars = HashMap<FeatureName, Rc<Variable>>;
/// Wrapper of russcip::Model, adding mappings from appropriate variable identifiers to their variables.
pub struct SolverModel<'a, State> {
    model: Model<State>,

    // Packages already placed in the previous freeze.
    preexisting_packages: &'a HashSet<ExpandedPackage>,
    // With what features they were placed there.
    preexisting_features: &'a HashMap<ExpandedPackage, HashSet<FeatureName>>,

    package_vars: HashMap<ExpandedPackage, Rc<Variable>>,
    package_to_feature_vars: HashMap<ExpandedPackage, FeaturesToVars>,
    dependency_to_feature_vars: HashMap<DependencyEdge, ChildFeaturesToVars>,
    dependency_to_version_vars: HashMap<DependencyEdge, ChildVersionsToVars>,
}

impl<'a> SolverModel<'a, ProblemCreated> {
    /// Creates an empty model, given packages already placed in the previous freeze and their features.
    pub fn new(
        preexisting_packages: &'a HashSet<ExpandedPackage>,
        preexisting_features: &'a HashMap<ExpandedPackage, HashSet<FeatureName>>,
    ) -> Self {
        SolverModel {
            model: Model::default().hide_output(),
            preexisting_packages,
            preexisting_features,
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
        pkg: &ExpandedPackage,
        feature: PresentFeature,
    ) -> QuackResult<Rc<Variable>> {
        match feature {
            None => self.package_vars.get(pkg).cloned().context_internal(
                "Package variable was not added to the model before retrieval attempt",
            ),
            Some(feature) => {
                let feature_to_var_map = self.package_to_feature_vars.get(&pkg).context_internal("Package and feature variable was not added to the model before retrieval attempt")?;
                feature_to_var_map.get(&feature).cloned().context_internal("Package and feature variable was not added to the model before retrieval attempt")
            }
        }
    }

    /// Returns the mapping from child features to variables, associated with the given dependency.
    fn get_feature_to_var_map_for_dep(
        &mut self,
        dep: &DependencyEdge,
    ) -> QuackResult<&ChildFeaturesToVars> {
        if matches!(self.dependency_to_feature_vars.get(dep), None) {
            self.dependency_to_feature_vars.insert(dep.clone(), HashMap::new());
        }
        self.dependency_to_feature_vars.get(dep).context_internal("We have just added an empty map")
    }

    /// Returns the variable associated with the given (dependency, child feature) pair.
    fn get_dependency_feature_variable(
        &mut self,
        dep: &DependencyEdge,
        feature: FeatureName,
    ) -> QuackResult<Rc<Variable>> {
        let feature_to_var_map = self.get_feature_to_var_map_for_dep(dep)?;
        feature_to_var_map.get(&feature).cloned().context_internal(
            "Dependency and feature variable not added to the model before retrieval of variable attempt",
        )
    }

    /// Returns the mapping from child versions to variables, associated with the given dependency.
    fn get_version_to_var_map_for_dep(
        &mut self,
        dep: &DependencyEdge,
    ) -> QuackResult<&ChildVersionsToVars> {
        if matches!(self.dependency_to_version_vars.get(dep), None) {
            self.dependency_to_version_vars.insert(dep.clone(), HashMap::new());
        }
        self.dependency_to_version_vars.get(dep).context_internal("We have just added an empty map")
    }

    /// Returns the variable associated with the given (dependency, child version) pair.
    fn get_dependency_version_variable(
        &mut self,
        dep: &DependencyEdge,
        version: Option<Version>,
    ) -> QuackResult<Rc<Variable>> {
        let version_to_var_map = self.get_version_to_var_map_for_dep(dep)?;
        version_to_var_map.get(&version).cloned().context_internal(
            "Dependency and version variable not added to the model before retrieval attempt",
        )
    }

    /// Creates the variable associated with the package and adds it to the model.
    pub fn add_package_var(&mut self, pkg: ExpandedPackage) -> QuackResult<()> {
        let objective_coef = if self.preexisting_packages.contains(&pkg) {
            0.0
        } else {
            1.0
        };
        let var_name = package_var_name(&pkg);
        self.package_vars.entry(pkg).or_insert_with(|| {
            Rc::new(
                self.model
                    .add(var().name(&var_name).bin().obj(objective_coef)),
            )
        });
        Ok(())
    }

    /// Creates the variable associated with the (package, feature) pair and adds it to the model.
    pub fn add_package_with_feature_var(
        &mut self,
        pkg: ExpandedPackage,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var_name = package_with_feature_var_name(&pkg, feature);
        self.package_to_feature_vars
            .entry(pkg)
            .or_insert_with(|| FeaturesToVars::new())
            .entry(feature)
            .or_insert_with(|| Rc::new(self.model.add(var().name(&var_name).bin().obj(0.0))));
        Ok(())
    }

    /// Creates a variable associated with the (dependency, child feature) pair and adds it to the model.
    pub fn add_dependency_feature_realisation_var(
        &mut self,
        dep: DependencyEdge,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var_name = dependency_feature_var_name(&dep, feature);
        self.dependency_to_feature_vars
            .entry(dep)
            .or_insert_with(|| ChildFeaturesToVars::new())
            .entry(feature)
            .or_insert_with(|| Rc::new(self.model.add(var().name(&var_name).bin().obj(0.0))));
        Ok(())
    }

    /// Creates a variable associated with the (dependency, child version) pair and adds it to the model.
    pub fn add_dependency_version_realisation_var(
        &mut self,
        dep: DependencyEdge,
        version: Option<Version>,
    ) -> QuackResult<()> {
        let var_name = dependency_version_var_name(&dep, version);
        self.dependency_to_version_vars
            .entry(dep)
            .or_insert(ChildVersionsToVars::new())
            .entry(version)
            .or_insert_with(|| Rc::new(self.model.add(var().name(&var_name).bin().obj(0.0))));
        Ok(())
    }

    /// Adds a constraint that forces the package to be present in the solution to the model.
    pub fn require_package(&mut self, pkg: &ExpandedPackage) -> QuackResult<()> {
        let var = self.get_package_variable(&pkg, None)?;
        self.model.add(cons().coef(&var, 1.0).eq(1.0));
        Ok(())
    }

    /// Adds a constaint that forces the package to be present with a feature.
    pub fn require_package_with_feature(
        &mut self,
        pkg: &ExpandedPackage,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var = self.get_package_variable(&pkg, Some(feature))?;
        self.model.add(cons().coef(&var, 1.0).eq(1.0));
        Ok(())
    }

    /// Adds a constraint that forces the child to be present with all of the required features.
    pub fn require_satisfying_dep_feature(
        &mut self,
        edge: &DependencyEdge,
        parent_feature: PresentFeature,
        child_features: &HashSet<FeatureName>,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(&edge.parent, parent_feature)?;
        let feature_vars = child_features
            .iter()
            .map(|feature| self.get_dependency_feature_variable(&edge, *feature))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        self.model.implies_one_all(parent_var, feature_vars);
        Ok(())
    }

    /// Adds a constraint that forces the child to be present in at least one of the required versions.
    pub fn require_satisfying_dep_version(
        &mut self,
        edge: &DependencyEdge,
        parent_feature: PresentFeature,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(&edge.parent, parent_feature)?;
        let version_vars = self
            .get_version_to_var_map_for_dep(&edge)?
            .values()
            .cloned()
            .collect();
        self.model.implies_all_any(vec![parent_var], version_vars);
        Ok(())
    }

    /// Adds a constraint that if the variable associated with a pair (dependency, child version)
    /// was chosen by the model to be present, the underlying package variable of the child in that version
    /// has to be present.
    pub fn require_substantiate_dep(&mut self, edge: &DependencyEdge) -> QuackResult<()> {
        for (pkg_version, version_realization_var) in
            self.get_version_to_var_map_for_dep(&edge)?.clone()
        {
            let pkg_var = self.get_package_variable(
                &ExpandedPackage {
                    location: edge.dependency_loc.clone(),
                    version: pkg_version,
                },
                None,
            )?;
            self.model.implies(version_realization_var.clone(), pkg_var);
        }
        Ok(())
    }

    /// Adds a constraint that if the variable associated with a pair (dependency, child version) was chosen to be present
    /// and the variable associated with a pair (dependency, child's feature) was chosen to be present,
    /// then the underlying package variable of the child in that version with that feature has to be present.
    pub fn require_substantiate_dep_features(
        &mut self,
        dep: &DependencyEdge,
        possible_features: &HashMap<ExpandedPackage, HashSet<FeatureName>>,
        possible_dep_realisations: Vec<ExpandedPackage>,
    ) -> QuackResult<()> {
        for pkg in possible_dep_realisations {
            let version_realization_var =
                self.get_dependency_version_variable(&dep, pkg.version())?;
            for (feature, feature_realization_var) in
                self.get_feature_to_var_map_for_dep(&dep)?.clone()
            {
                if !possible_features
                    .get(&pkg)
                    .context_internal("Possible features map does not contain looked up package")?
                    .contains(&feature)
                {
                    self.model.implies_all_any(
                        vec![
                            version_realization_var.clone(),
                            feature_realization_var.clone(),
                        ],
                        vec![],
                    );
                } else {
                    self.model.implies_all_any(
                        vec![
                            version_realization_var.clone(),
                            feature_realization_var.clone(),
                        ],
                        vec![self.get_package_variable(&pkg, Some(feature))?],
                    );
                }
            }
        }
        Ok(())
    }
}

/// Output of the solver model, contains new packages to be put into the freeze, with their features.
pub struct FoundSolution {
    pub new_packages: HashSet<ExpandedPackage>,
    pub new_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
}

impl<'a> SolverModel<'a, ProblemCreated> {
    /// Given a constructed problem (with variables and constraints added), calls SCIP to solve it,
    /// constructs the output and returns it.
    pub fn solve(self) -> FoundSolution {
        self.model.minimize().solve();
        let new_packages = self
            .package_vars
            .into_iter()
            .filter_map(|(pkg, var)| if is_one(&var) { Some(pkg) } else { None })
            .collect::<HashSet<ExpandedPackage>>()
            .difference(self.preexisting_packages)
            .cloned()
            .collect::<HashSet<ExpandedPackage>>();
        let mut new_features = HashMap::new();
        for pkg in self.preexisting_packages.iter().chain(new_packages.iter()) {
            let mut pkg_features = HashSet::new();
            if let Some(features_to_vars) = self.package_to_feature_vars.get(pkg) {
                pkg_features = features_to_vars
                    .iter()
                    .filter_map(
                        |(feature, var)| {
                            if is_one(var) { Some(feature) } else { None }
                        },
                    )
                    .cloned()
                    .collect();
            }
            if let Some(features) = self.preexisting_features.get(pkg) {
                pkg_features = pkg_features.difference(features).cloned().collect();
            }
            new_features.insert(pkg.clone(), pkg_features);
        }
        FoundSolution {
            new_packages,
            new_features,
        }
    }
}

/// Helper function for determining whether the model did or did not put the variable into the solution.
fn is_one(var: &Variable) -> bool {
    // The condition "> 0.5" is arbitrary, ideally it could be "== 1.0", but maybe to circumvent some float magic "> 0.5" is better.
    var.sol_val() > 0.5
}
