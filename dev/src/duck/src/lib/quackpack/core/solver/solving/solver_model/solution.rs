// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for translating the solution to the ILP
//! into information which packages and features should be added to the freeze.

use std::collections::{HashMap, HashSet};
use std::rc::Rc;

use russcip::{Solution, Variable};
use tracing::trace;

use crate::quackpack::core::solver::SolverInput;
use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
use crate::quackpack::core::solver::solving::solver_model::FeaturesToVars;
use crate::quackpack::core::{FeatureName, PackageId, Selector, Version};
use crate::{QuackResult, qp_bail_internal};

// @TODO: #3986 simplify this, return the whole SolverFreeze here.
/// Output of the solver model, contains new packages to be put into the freeze, with their features.
#[derive(Debug)]
pub struct FoundSolution {
    pub new_packages: HashSet<PackageId>,
    pub new_features: HashMap<PackageId, HashSet<FeatureName>>,
    pub new_edges: HashMap<DependencyEdge, Version>,
}

/// Helper for determining which new packages have been chosen by the model.
pub(super) fn new_packages(
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
pub(super) fn new_features(
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
                .copied()
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
pub(super) fn new_edges(
    new_packages: &HashSet<PackageId>,
    new_features: &HashMap<PackageId, HashSet<FeatureName>>,
    preexisting_packages: &HashSet<PackageId>,
    input: &SolverInput,
) -> QuackResult<HashMap<DependencyEdge, Version>> {
    let mut new_edges = HashMap::new();
    for pkg in preexisting_packages.iter().chain(new_packages.iter()) {
        let pkg_data = input.package_data(*pkg)?;
        let mut features = new_features.get(pkg).cloned().unwrap_or_default();
        features.extend(
            input
                .preexisting_features_for_pkg(*pkg)
                .into_iter()
                .flatten()
                .copied(),
        );
        for dep in pkg_data
            .manifest()
            .dependencies()
            .select(&Selector::EnabledBy(&features))
        {
            if pkg_data
                .dependency_preexists(dep.effective_name())
                .is_some()
            {
                continue;
            }
            let Some(edge) = DependencyEdge::from_manifest_and_parent(
                *pkg,
                dep,
                &input.source_to_origin_resolver,
            ) else {
                qp_bail_internal!("no dependency edge for {pkg:?} and {dep:?}");
            };
            for version in input.all_versions(edge.dep_identity)? {
                let realization = PackageId::new(edge.dep_identity, *version);
                if preexisting_packages.contains(&realization)
                    || new_packages.contains(&realization)
                {
                    new_edges.insert(edge, *version);
                    break;
                }
            }
        }
    }
    Ok(new_edges)
}

/// Helper function for determining whether the model did or did not put the variable into the solution.
fn is_one(var: &Variable, solution: &Solution) -> bool {
    // The condition "> 0.5" is arbitrary, ideally it could be "== 1.0", but maybe to circumvent some float magic "> 0.5" is better.
    solution.val(var) > 0.5
}
