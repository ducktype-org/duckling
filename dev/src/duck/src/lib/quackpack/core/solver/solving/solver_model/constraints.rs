// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! All possible types of constraints that occur when resolving dependencies.
//! Adds functions to [`SolverModel`] which translate every type of constraint into an ILP inequality.

use std::format;
use std::rc::Rc;

use russcip::builder::cons::{ConsBuilder as RawConstraint, cons};
use russcip::{ProblemCreated, Variable};

use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
use crate::quackpack::core::solver::solving::scip_ext::ConsBuilderExt;
use crate::quackpack::core::solver::solving::solver_model::{PresentFeature, SolverModel};
use crate::quackpack::core::{FeatureName, PackageId, Version};
use crate::{QuackResult, StrId};

#[allow(dead_code)]
#[derive(Debug)]
/// All kinds of constraints added to the ILP for resolving dependencies.
/// This is overly verbose on purpose, so that in the future we can provide good explanation through minimal infeasible subproblem.
pub enum Constraint {
    /// The root package must occur in the solution.
    RequireRootPackage,
    /// The root package must occur in the solution with this feature.
    RequireRootFeature(FeatureName),
    /// Dependency's parent must not occur in the solution, because no realization of the dependency was found.
    /// Note
    /// ----
    /// Possible only in merciful mode.
    ForbidPackageBecauseNoRealization { pkg: PackageId, dep_dname: StrId },
    /// This package must not occur in the solution with this feature,
    /// because no realization of the dependency was found.
    /// Note
    /// ----
    /// Possible only in merciful mode.
    ForbidPackageFeaturesBecauseNoRealization {
        pkg: PackageId,
        dep_dname: StrId,
        feature: FeatureName,
    },
    /// This package with both features has an enabled dependency which cannot be realized by `realization`.
    /// This is because `forcing_feature` forces some unsupported feature of realization.
    ForbidFeatureBecauseForces {
        pkg: PackageId,
        enabling_feature: PresentFeature,
        forcing_feature: PresentFeature,
        realization: PackageId,
    },
    /// This package must not occur in the solution with this feature,
    /// because parent is preexistent, but `feature` forces some feature which realization does not support.
    /// Note
    /// ----
    /// Possible only when reusing previous freeze.
    ForbidPreexistingFeatureBecauseForces {
        pkg: PackageId,
        realization: PackageId,
        feature: FeatureName,
    },
    /// If parent of the dependency is present then at least one realization has to be present as well.
    RequireDepRealization {
        dep: DependencyEdge,
        parent_feature: PresentFeature,
        realizations: Vec<PackageId>,
    },
    /// If parent of the dependency is present, some realization was chosen and the dependency forces some features,
    /// then realization has to occur with those features.
    RequireForcedFeature {
        dep: DependencyEdge,
        enabling_feature: PresentFeature,
        forcing_feature: PresentFeature,
        realization: Version,
        forced_features: Vec<FeatureName>,
    },
    /// Non-preexistent feature of a preexistent package forces some features of realization.
    /// Note
    /// ----
    /// Possible only when reusing previous freeze.
    RequireForcedFeatureForPreexisting {
        parent: PackageId,
        parent_feature: FeatureName,
        realization: PackageId,
        forced_features: Vec<FeatureName>,
    },
    /// If package is present with feature, then it must have features which it expands into.
    RequireFeatureExpansion {
        pkg: PackageId,
        feature: FeatureName,
        expands_to: Vec<FeatureName>,
    },
    /// Forbid two packages with the same [`Identity`] from being chosen simultaneously.
    ForbidMultipleVersions {
        id: Identity,
        versions: Vec<PackageId>,
    },
    /// Forbid packages with a given [`Identity`] and different from a given [`PackageId`] from being chosen.
    /// This should be used when some package with the given [`Identity`] preexists.
    ForbidDifferentVersions {
        id: Identity,
        different_versions: Vec<PackageId>,
        preexistent_version: PackageId,
    },
}

impl<'a> SolverModel<'a, ProblemCreated> {
    /// Add a constraint to the model.
    fn add_constraint<'b>(&mut self, constraint: Constraint, scip_constraint: RawConstraint<'b>) {
        let id = self.constraints.len();
        let name = format!("CONSTRAINT_{id}");
        let scip_constraint = scip_constraint.name(&name);
        self.model.add(scip_constraint);
        self.constraints.push(constraint);
    }

    /// Add a constraint that forces the package to be present in the solution to the model.
    pub fn require_root_package(&mut self, pkg: PackageId) -> QuackResult<()> {
        let constraint = Constraint::RequireRootPackage();
        let var = self.get_package_variable(pkg, None)?;
        let scip_constraint = cons().coef(&var, 1.0).eq(1.0);
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Add a constraint that forces the package to be present with a feature.
    pub fn require_root_package_feature(
        &mut self,
        pkg: PackageId,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let constraint = Constraint::RequireRootFeature(feature);
        let var = self.get_package_variable(pkg, Some(feature))?;
        let scip_constraint = cons().coef(&var, 1.0).eq(1.0);
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Add a constraint that this package can never be present.
    pub fn forbid_package_because_no_realization(
        &mut self,
        pkg: PackageId,
        dep_dname: StrId,
    ) -> QuackResult<()> {
        let constraint = Constraint::ForbidPackageBecauseNoRealization { pkg, dep_dname };
        let var = self.get_package_variable(pkg, None)?;
        let scip_constraint = cons().coef(&var, 1.0).eq(0.0);
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Add a constraint that this package can never be present with both features.
    pub fn forbid_package_feature_because_no_realization(
        &mut self,
        pkg: PackageId,
        feature: FeatureName,
        dep_dname: StrId,
    ) -> QuackResult<()> {
        let var = self.get_package_variable(pkg, Some(feature))?;
        let scip_constraint = cons().coef(&var, 1.0).eq(0.0);
        let constraint = Constraint::ForbidPackageFeaturesBecauseNoRealization {
            pkg,
            feature,
            dep_dname,
        };
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Add a constraint that realization and this package can never be present with both features.
    pub fn forbid_package_feature_because_forces(
        &mut self,
        pkg: PackageId,
        enabling_feature: PresentFeature,
        forcing_feature: PresentFeature,
        realization: PackageId,
    ) -> QuackResult<()> {
        let constraint = Constraint::ForbidFeatureBecauseForces {
            pkg,
            enabling_feature,
            forcing_feature,
            realization,
        };
        let enabling_var = self.get_package_variable(pkg, enabling_feature)?;
        let forcing_var = self.get_package_variable(pkg, forcing_feature)?;
        let realization_var = self.get_package_variable(realization, None)?;
        let scip_constraint = RawConstraint::all_implies_any(
            vec![
                enabling_var.as_ref(),
                forcing_var.as_ref(),
                realization_var.as_ref(),
            ],
            vec![],
        );
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Add a constraint that this package can never be present with the given feature.
    pub fn forbid_preexistent_package_feature_because_forces(
        &mut self,
        pkg: PackageId,
        realization: PackageId,
        feature: FeatureName,
    ) -> QuackResult<()> {
        let constraint = Constraint::ForbidPreexistingFeatureBecauseForces {
            pkg,
            realization,
            feature,
        };
        let var = self.get_package_variable(pkg, Some(feature))?;
        let scip_constraint = cons().coef(&var, 1.0).eq(0.0);
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Adds a constraint that forces the child to be present in at least one of the required versions,
    /// assuming parent is present with `parent_feature`.
    pub fn require_dep_realization(
        &mut self,
        dep: DependencyEdge,
        parent_feature: PresentFeature,
        possible_realizations: Vec<PackageId>,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(dep.parent, parent_feature)?;
        let version_vars = possible_realizations
            .iter()
            .map(|pkg| self.get_package_variable(*pkg, None))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        let scip_constraint = RawConstraint::all_implies_any(
            vec![parent_var.as_ref()],
            version_vars.iter().map(|x| x.as_ref()).collect(),
        );
        let constraint = Constraint::RequireDepRealization {
            dep,
            parent_feature,
            realizations: possible_realizations,
        };
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Adds a constraint that forces the the child to be present with forced features,
    /// assuming parent is present with `parent_enabling_feature` and `parent_forcing_feature`.
    pub fn require_forced_features(
        &mut self,
        dep: DependencyEdge,
        enabling_feature: PresentFeature,
        forcing_feature: PresentFeature,
        realization: Version,
        forced_features: Vec<FeatureName>,
    ) -> QuackResult<()> {
        let realization_pkg = PackageId::new(dep.dep_identity, realization);
        let enabling_var = self.get_package_variable(dep.parent, enabling_feature)?;
        let forcing_var = self.get_package_variable(dep.parent, forcing_feature)?;
        let realization_var = self.get_package_variable(realization_pkg, None)?;
        let forced_features_vars = forced_features
            .iter()
            .map(|f| self.get_package_variable(realization_pkg, Some(*f)))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        let scip_constraint = RawConstraint::all_implies_all(
            vec![
                enabling_var.as_ref(),
                forcing_var.as_ref(),
                realization_var.as_ref(),
            ],
            forced_features_vars.iter().map(|x| x.as_ref()).collect(),
        );
        let constraint = Constraint::RequireForcedFeature {
            dep,
            enabling_feature,
            forcing_feature,
            realization,
            forced_features,
        };
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Adds a constraint that forces the realization of a preexistent dependency,
    /// assuming parent is present with (not preexistent) `parent_feature`.
    pub fn require_forced_features_for_preexisting(
        &mut self,
        parent: PackageId,
        parent_feature: FeatureName,
        realization: PackageId,
        forced_features: Vec<FeatureName>,
    ) -> QuackResult<()> {
        let parent_var = self.get_package_variable(parent, Some(parent_feature))?;
        let forced_features_vars = forced_features
            .iter()
            .map(|f| self.get_package_variable(realization, Some(*f)))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        let scip_constraint = RawConstraint::all_implies_all(
            vec![parent_var.as_ref()],
            get_refs_vec(&forced_features_vars),
        );
        let constraint = Constraint::RequireForcedFeatureForPreexisting {
            parent,
            parent_feature,
            realization,
            forced_features,
        };
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Adds a constraint the pair (pkg, feature) implies the presence of pairs (pkg, F) for any feature F to which `feature` expands.
    pub fn require_features_expansion(
        &mut self,
        pkg: PackageId,
        feature: FeatureName,
        expands_to: Vec<FeatureName>,
    ) -> QuackResult<()> {
        let expands_to_vars = expands_to
            .iter()
            .map(|f| self.get_package_variable(pkg, Some(*f)))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        let feature_var = self.get_package_variable(pkg, Some(feature))?;
        let scip_constraint = RawConstraint::all_implies_all(
            vec![feature_var.as_ref()],
            get_refs_vec(&expands_to_vars),
        );
        let constraint = Constraint::RequireFeatureExpansion {
            pkg,
            feature,
            expands_to,
        };
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Forbid more that one version of the package to be chosen.
    pub fn forbid_more_that_one_version(
        &mut self,
        identity: Identity,
        versions: Vec<PackageId>,
    ) -> QuackResult<()> {
        let version_vars = versions
            .iter()
            .map(|pkg| self.get_package_variable(*pkg, None))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        let scip_constraint = RawConstraint::at_most_one(get_refs_vec(&version_vars));
        let constraint = Constraint::ForbidMultipleVersions {
            id: identity,
            versions,
        };
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }

    /// Forbid all versions of the package other than the preexisting one.
    pub fn forbid_versions_different_from_preexisting(
        &mut self,
        identity: Identity,
        preexistent_version: PackageId,
        different_versions: Vec<PackageId>,
    ) -> QuackResult<()> {
        let forbidden_version_vars = different_versions
            .iter()
            .map(|pkg| self.get_package_variable(*pkg, None))
            .collect::<QuackResult<Vec<Rc<Variable>>>>()?;
        let scip_constraint = RawConstraint::none(get_refs_vec(&forbidden_version_vars));
        let constraint = Constraint::ForbidDifferentVersions {
            id: identity,
            different_versions,
            preexistent_version,
        };
        self.add_constraint(constraint, scip_constraint);
        Ok(())
    }
}

/// Helper for getting a vector of references to [`Rc`]s inside a vector.
fn get_refs_vec(vec: &[Rc<Variable>]) -> Vec<&Variable> {
    vec.iter().map(|x| x.as_ref()).collect()
}
