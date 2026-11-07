use std::collections::HashSet;
use std::rc::Rc;

use russcip::builder::cons::cons;
use russcip::{ProblemCreated, Variable};

use crate::QuackResult;
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
use crate::quackpack::core::solver::solving::scip_ext::BinModelExt;
use crate::quackpack::core::solver::solving::solver_model::{PresentFeature, SolverModel};
use crate::quackpack::core::{FeatureName, PackageId, Version};

#[derive(Debug)]
pub(super) enum Constraint {
    /// This package must occur in the solution.
    RequirePackage(PackageId),
    /// This package must occur in the solution with this feature.
    RequirePackageWithFeature(PackageId, FeatureName),
    /// This package must not occur in the solution.
    ForbidPackage(PackageId),
    /// This package must not occur in the solution with this feature.
    ForbidPackageWithFeature(PackageId, FeatureName),
    /// If parent of the dependency is present then at least one realization has to be present as well.
    RequireDepPresence {
        dep: DependencyEdge,
        parent_feature: PresentFeature,
        realizations: Vec<Version>,
    },
    /// If parent of the dependency is present, some realization was chosen and the dependency forces some features,
    /// then realization has to occur with those features.
    RequireForcedFeature {
        dep: DependencyEdge,
        parent_feature: PresentFeature,
        realization: Version,
        forced_features: Vec<FeatureName>,
    },
    /// Prohibit any package from occuring in more than one version.
    ProhibitMultipleVersions {
        id: Identity,
        versions: Vec<Version>,
    },
}

impl<'a> SolverModel<'a, ProblemCreated> {
    /// Adds a constraint to the model.
    fn add_constraint(&mut self, constraint: Constraint) {
        let id = self.constraints.len();
        todo!("create russcip's constraint, add a name to it");
        self.constraints.push(constraint);
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
        possible_dep_realizations: &[PackageId],
    ) -> QuackResult<()> {
        for pkg in possible_dep_realizations {
            let version_realization_var =
                self.get_dependency_version_variable(dep, pkg.version())?;
            for (feature, feature_realization_var) in
                self.get_feature_to_var_map_for_dep(dep).clone()
            {
                let pkg_features = self.input.package_data(*pkg)?.features();
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

    /// Forbid more that one version of the package to be chosen.
    pub fn forbid_more_that_one_version(
        &mut self,
        package_versions: Vec<PackageId>,
    ) -> QuackResult<()> {
        let version_vars: QuackResult<Vec<Rc<Variable>>> = package_versions
            .into_iter()
            .map(|pkg| self.get_package_variable(pkg, None))
            .collect();
        let version_vars = version_vars?;
        self.model.at_most_one(version_vars);
        Ok(())
    }
}
