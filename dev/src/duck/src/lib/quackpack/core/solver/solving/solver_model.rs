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
            types_common::{ExpandedPackage, ParentWithDependencyLoc, PresentFeature},
        },
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

type FeaturesToVars = HashMap<FeatureName, Rc<Variable>>;
type ChildVersionsToVars = HashMap<Option<Version>, Rc<Variable>>;
type ChildFeaturesToVars = HashMap<FeatureName, Rc<Variable>>;
pub struct SolverModel<'a, State> {
    model: Model<State>,
    preexisting_packages: &'a HashSet<ExpandedPackage>,
    preexisting_features: &'a HashMap<ExpandedPackage, HashSet<FeatureName>>,
    package_vars: HashMap<ExpandedPackage, Rc<Variable>>,
    package_to_feature_vars: HashMap<ExpandedPackage, FeaturesToVars>,
    dependency_to_feature_vars: HashMap<ParentWithDependencyLoc, ChildFeaturesToVars>,
    dependency_to_version_vars: HashMap<ParentWithDependencyLoc, ChildVersionsToVars>,
}

impl<'a> SolverModel<'a, ProblemCreated> {
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

    fn get_feature_to_var_map_for_dep(
        &self,
        dep: &ParentWithDependencyLoc,
    ) -> QuackResult<&ChildFeaturesToVars> {
        self.dependency_to_feature_vars.get(&dep).context_internal(
            "Dependency and feature variable not added to the model before retrieval attempt",
        )
    }

    fn get_dependency_feature_variable(
        &self,
        dep: &ParentWithDependencyLoc,
        feature: FeatureName,
    ) -> QuackResult<Rc<Variable>> {
        let feature_to_var_map = self.get_feature_to_var_map_for_dep(dep)?;
        feature_to_var_map.get(&feature).cloned().context_internal(
            "Dependency and feature variable not added to the model before retrieval attempt",
        )
    }

    fn get_version_to_var_map_for_dep(
        &self,
        dep: &ParentWithDependencyLoc,
    ) -> QuackResult<&ChildVersionsToVars> {
        self.dependency_to_version_vars.get(&dep).context_internal(
            "Dependency and version variable not added to the model before retrieval attempt",
        )
    }

    fn get_dependency_version_variable(
        &self,
        dep: &ParentWithDependencyLoc,
        version: Option<Version>,
    ) -> QuackResult<Rc<Variable>> {
        let version_to_var_map = self.get_version_to_var_map_for_dep(dep)?;
        version_to_var_map.get(&version).cloned().context_internal(
            "Dependency and version variable not added to the model before retrieval attempt",
        )
    }

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

    pub fn add_dependency_feature_realisation_var(
        &mut self,
        dep: ParentWithDependencyLoc,
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

    pub fn add_dependency_version_realisation_var(
        &mut self,
        dep: ParentWithDependencyLoc,
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

    pub fn require_package(&mut self, pkg: &ExpandedPackage) -> QuackResult<()> {
        let var = self.get_package_variable(&pkg, None)?;
        self.model.add(cons().coef(&var, 1.0).eq(1.0));
        Ok(())
    }

    pub fn require_package_with_feature(
        &mut self,
        pkg: &ExpandedPackage,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var = self.get_package_variable(&pkg, Some(feature))?;
        self.model.add(cons().coef(&var, 1.0).eq(1.0));
        Ok(())
    }

    pub fn require_satisfying_dep_feature(
        &mut self,
        dep: &ParentWithDependencyLoc,
        parent_feature: PresentFeature,
        child_features: HashSet<FeatureName>,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(&dep.parent, parent_feature)?;
        let feature_vars = child_features
            .iter()
            .map(|feature| self.get_dependency_feature_variable(&dep, *feature))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        self.model.implies_one_all(parent_var, feature_vars);
        Ok(())
    }

    pub fn require_satisfying_dep_version(
        &mut self,
        dep: &ParentWithDependencyLoc,
        parent_feature: PresentFeature,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(&dep.parent, parent_feature)?;
        let version_vars = self
            .get_version_to_var_map_for_dep(&dep)?
            .values()
            .cloned()
            .collect();
        self.model.implies_all_any(vec![parent_var], version_vars);
        Ok(())
    }

    pub fn require_substantiate_dep(&mut self, dep: &ParentWithDependencyLoc) -> QuackResult<()> {
        for (pkg_version, version_realization_var) in
            self.get_version_to_var_map_for_dep(&dep)?.clone()
        {
            let pkg_var = self.get_package_variable(
                &ExpandedPackage {
                    location: dep.dependency_loc.clone(),
                    version: pkg_version,
                },
                None,
            )?;
            self.model.implies(version_realization_var.clone(), pkg_var);
        }
        Ok(())
    }

    pub fn require_substantiate_dep_features(
        &mut self,
        dep: &ParentWithDependencyLoc,
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

pub struct FoundSolution {
    pub new_packages: HashSet<ExpandedPackage>,
    pub new_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
}

impl<'a> SolverModel<'a, ProblemCreated> {
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

fn is_one(var: &Variable) -> bool {
    var.sol_val() > 0.5
}
