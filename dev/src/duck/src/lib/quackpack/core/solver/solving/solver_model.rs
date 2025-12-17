use std::{
    cell::{Ref, RefCell, RefMut},
    collections::{HashMap, HashSet},
};

use russcip::{
    Model, ProblemCreated, Variable,
    prelude::{cons, var},
};

use crate::{
    QuackResult, QuackResultContext, StrId, qp_internal,
    quackpack::core::{
        FeatureName, Version,
        solver::{
            solving::scip_ext::BinModelExt,
            types::{ExpandedPackage, ParentWithDependencyLoc, PresentFeature},
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

type FeaturesToVars = HashMap<FeatureName, Variable>;
type ChildVersionsToVars = HashMap<Option<Version>, Variable>;
type ChildFeaturesToVars = HashMap<FeatureName, Variable>;
pub struct SolverModel<State> {
    model: RefCell<Model<State>>,
    package_vars: RefCell<HashMap<ExpandedPackage, Variable>>,
    package_to_feature_vars: RefCell<HashMap<ExpandedPackage, FeaturesToVars>>,
    dependency_to_feature_vars: RefCell<HashMap<ParentWithDependencyLoc, ChildFeaturesToVars>>,
    dependency_to_version_vars: RefCell<HashMap<ParentWithDependencyLoc, ChildVersionsToVars>>,
}

impl<State> SolverModel<State> {
    fn model_mut(&self) -> QuackResult<RefMut<'_, Model<State>>> {
        self.model
            .try_borrow_mut()
            .context_internal("Conflicting references to solver model's inner model")
    }

    fn model(&self) -> QuackResult<Ref<'_, Model<State>>> {
        self.model
            .try_borrow()
            .context_internal("Conflicting references to solver model's inner model")
    }

    fn package_vars_mut(&self) -> QuackResult<RefMut<'_, HashMap<ExpandedPackage, Variable>>> {
        self.package_vars.try_borrow_mut().context_internal(
            "Conflicting references to solver model's inner package to variables map",
        )
    }

    fn package_vars(&self) -> QuackResult<Ref<'_, HashMap<ExpandedPackage, Variable>>> {
        self.package_vars.try_borrow().context_internal(
            "Conflicting references to solver model's inner package to variables map",
        )
    }

    fn package_to_feature_vars_mut(
        &self,
    ) -> QuackResult<RefMut<'_, HashMap<ExpandedPackage, FeaturesToVars>>> {
        self.package_to_feature_vars.try_borrow_mut()
            .context_internal("Conflicting references to solver model's inner package to features to variables map")
    }

    fn package_to_feature_vars(
        &self,
    ) -> QuackResult<Ref<'_, HashMap<ExpandedPackage, FeaturesToVars>>> {
        self.package_to_feature_vars.try_borrow().context_internal(
            "Conflicting references to solver model's inner package to features to variables map",
        )
    }

    fn dependency_to_feature_vars_mut(
        &self,
    ) -> QuackResult<RefMut<'_, HashMap<ParentWithDependencyLoc, ChildFeaturesToVars>>> {
        self.dependency_to_feature_vars.try_borrow_mut()
            .context_internal("Conflicting references to solver model's inner dependency to feature to variables map")
    }

    fn dependency_to_feature_vars(
        &self,
    ) -> QuackResult<Ref<'_, HashMap<ParentWithDependencyLoc, ChildFeaturesToVars>>> {
        self.dependency_to_feature_vars.try_borrow()
            .context_internal("Conflicting references to solver model's inner dependency to feature to variables map")
    }

    fn dependency_to_version_vars_mut(
        &self,
    ) -> QuackResult<RefMut<'_, HashMap<ParentWithDependencyLoc, ChildVersionsToVars>>> {
        self.dependency_to_version_vars.try_borrow_mut()
            .context_internal("Conflicting references to solver model's inner dependency to version to variables map")
    }

    fn dependency_to_version_vars(
        &self,
    ) -> QuackResult<Ref<'_, HashMap<ParentWithDependencyLoc, ChildVersionsToVars>>> {
        self.dependency_to_version_vars.try_borrow()
            .context_internal("Conflicting references to solver model's inner dependency to version to variables map")
    }
}

impl SolverModel<ProblemCreated> {
    pub fn new() -> Self {
        SolverModel {
            model: RefCell::new(Model::default().hide_output()),
            package_vars: RefCell::new(HashMap::new()),
            package_to_feature_vars: RefCell::new(HashMap::new()),
            dependency_to_version_vars: RefCell::new(HashMap::new()),
            dependency_to_feature_vars: RefCell::new(HashMap::new()),
        }
    }

    fn get_package_variable(
        &self,
        pkg: &ExpandedPackage,
        feature: PresentFeature,
    ) -> QuackResult<Ref<Variable>> {
        match feature {
            None => Ref::filter_map(self.package_vars()?, |r| r.get(&pkg)).map_err(|_| {
                qp_internal!("Package variable was not added to the model before retrieval attempt")
            }),
            Some(feature) => {
                let feature_to_var_ref = Ref::filter_map(self.package_to_feature_vars()?,
                |r| r.get(&pkg)).map_err(|_| qp_internal!("Package and feature variable was not added to the model before retrieval attempt"))?;

                Ref::filter_map(feature_to_var_ref, |r|
                r.get(&feature)).map_err(|_| qp_internal!("Package and feature variable was not added to the model before retrieval attempt"))
            }
        }
    }

    fn get_feature_to_var_map_for_dep(
        &self,
        dep: &ParentWithDependencyLoc,
    ) -> QuackResult<Ref<ChildFeaturesToVars>> {
        Ref::filter_map(self.dependency_to_feature_vars()?, |r| r.get(&dep)).map_err(|_| {
            qp_internal!(
                "Dependency and feature variable not added to the model before retrieval attempt"
            )
        })
    }

    fn get_dependency_feature_variable(
        &self,
        dep: &ParentWithDependencyLoc,
        feature: FeatureName,
    ) -> QuackResult<Ref<Variable>> {
        let feature_to_var_map = self.get_feature_to_var_map_for_dep(dep)?;
        Ref::filter_map(feature_to_var_map, |r| r.get(&feature)).map_err(|_| {
            qp_internal!(
                "Dependency and feature variable not added to the model before retrieval attempt"
            )
        })
    }

    pub fn add_package_var(&mut self, pkg: ExpandedPackage) -> QuackResult<()> {
        let var_name = package_var_name(&pkg);
        self.package_vars_mut()?
            .entry(pkg)
            .or_insert_with(|| self.model.borrow_mut().add(var().name(&var_name).bin()));
        Ok(())
    }

    pub fn add_package_with_feature_var(
        &mut self,
        pkg: ExpandedPackage,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var_name = package_with_feature_var_name(&pkg, feature);
        self.package_to_feature_vars_mut()?
            .entry(pkg)
            .or_insert_with(|| FeaturesToVars::new())
            .entry(feature)
            .or_insert_with(|| self.model.borrow_mut().add(var().name(&var_name).bin()));
        Ok(())
    }

    pub fn add_dependency_feature_realisation_var(
        &mut self,
        dep: ParentWithDependencyLoc,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var_name = dependency_feature_var_name(&dep, feature);
        self.dependency_to_feature_vars_mut()?
            .entry(dep)
            .or_insert_with(|| ChildFeaturesToVars::new())
            .entry(feature)
            .or_insert_with(|| self.model.borrow_mut().add(var().name(&var_name).bin()));
        Ok(())
    }

    pub fn add_dependency_version_realisation_var(
        &mut self,
        dep: ParentWithDependencyLoc,
        version: Option<Version>,
    ) -> QuackResult<()> {
        let var_name = dependency_version_var_name(&dep, version);
        self.dependency_to_version_vars_mut()?
            .entry(dep)
            .or_insert(ChildVersionsToVars::new())
            .entry(version)
            .or_insert_with(|| self.model.borrow_mut().add(var().name(&var_name).bin()));
        Ok(())
    }

    pub fn require_package(&self, pkg: ExpandedPackage) -> QuackResult<()> {
        let var = self.get_package_variable(&pkg, None)?;
        self.model.borrow_mut().add(cons().coef(&var, 1.0).eq(1.0));
        Ok(())
    }

    pub fn require_package_with_feature(
        &self,
        pkg: ExpandedPackage,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let var = self.get_package_variable(&pkg, Some(feature))?;
        self.model_mut()?.add(cons().coef(&var, 1.0).eq(1.0));
        Ok(())
    }

    pub fn require_satisfy_dep_feature(
        &self,
        dep: ParentWithDependencyLoc,
        parent_feature: PresentFeature,
        child_features: HashSet<FeatureName>,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(&dep.parent, parent_feature)?;
        let feature_vars: QuackResult<Vec<Ref<Variable>>> = child_features.iter().map(|feature| 
        self.get_dependency_feature_variable(&dep, *feature)).collect();
        self.model_mut()?.implies_one_all(parent_var, feature_vars);
        Ok(())
    }
}
