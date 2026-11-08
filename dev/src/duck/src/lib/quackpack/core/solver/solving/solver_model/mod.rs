// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module containing a wrapper over [`Model`], with utilities related to dependency resolving.
//! Note:
//! -----
//! By `child` in the context of a given dependency relation we mean the package realising that dependency.
use std::collections::HashMap;
use std::rc::Rc;

use russcip::prelude::var;
use russcip::{Model, ProblemCreated, Variable, WithSolutions};
use tracing::debug;

use crate::quackpack::core::solver::solving::input::SolverInput;
use crate::quackpack::core::solver::solving::solver_model::solution::{
    new_edges, new_features, new_packages,
};
use crate::quackpack::core::{FeatureName, PackageId};
use crate::{QuackResult, QuackResultContext, StrId, qp_bail};

mod constraints;
mod solution;

pub use constraints::Constraint;
pub use solution::FoundSolution;

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

type FeaturesToVars = HashMap<FeatureName, Rc<Variable>>;

#[derive(Debug)]
/// Wrapper of [`Model`], adding mappings from appropriate variable identifiers to their variables.
pub struct SolverModel<'a, State> {
    model: Model<State>,

    input: &'a SolverInput,

    package_vars: HashMap<PackageId, Rc<Variable>>,
    package_to_feature_vars: HashMap<PackageId, FeaturesToVars>,

    constraints: Vec<Constraint>,
}

impl<'a> SolverModel<'a, ProblemCreated> {
    /// Cost of putting a preexisting package into the solution.
    /// Zero, because preexisting packages are already treated as in the solution.
    const PREEXISTING_PKG_COST_FUNCTION: f64 = 0.0;
    /// Cost of putting a new package into the solution.
    /// This is the main objective we want to minimize.
    const PKG_COST_FUNTION: f64 = 1.0;
    /// Cost of putting a new feature into the solution.
    /// We want to minimize the number of such additions as well,
    /// but minimizing the number of new packages is the priority.
    const FEATURE_COST_FUNCITON: f64 = 0.001;

    /// Creates an empty model, given packages already placed in the previous freeze and their features.
    pub fn new(input: &'a SolverInput) -> Self {
        SolverModel {
            model: Model::default().hide_output(),
            input,
            package_vars: HashMap::new(),
            package_to_feature_vars: HashMap::new(),
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

    /// Creates the variable associated with the package and adds it to the model.
    pub fn add_package_var(&mut self, pkg: PackageId) {
        let objective_coef = if self.input.preexists(pkg) {
            Self::PREEXISTING_PKG_COST_FUNCTION
        } else {
            Self::PKG_COST_FUNTION
        };
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
            .or_insert_with(|| {
                Rc::new(
                    self.model
                        .add(var().name(&var_name).bin().obj(Self::FEATURE_COST_FUNCITON)),
                )
            });
    }
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
        let new_edges = new_edges(
            &new_packages,
            &new_features,
            &preexisting_packages,
            self.input,
        )?;
        Ok(FoundSolution {
            new_packages,
            new_features,
            new_edges,
        })
    }
}
