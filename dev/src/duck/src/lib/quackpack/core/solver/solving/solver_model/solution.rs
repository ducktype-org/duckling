// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for constructing a new [`SolverFreeze`], based on the solution to the ILP describing dependency constraints.

use std::collections::{HashMap, HashSet};
use std::rc::Rc;

use russcip::{Solution, Variable};
use tracing::trace;

use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
use crate::quackpack::core::solver::solving::input::SolverInput;
use crate::quackpack::core::solver::solving::solver_model::FeaturesToVars;
use crate::quackpack::core::{FeatureName, PackageId, Selector};
use crate::{QuackResult, QuackResultContext, StrId, qp_bail_internal};

/// Get a [`SolverFreeze`] from [`SolverInput`] and [`Solution`].
pub(super) fn freeze_from_solution(
    package_vars: HashMap<PackageId, Rc<Variable>>,
    package_to_feature_vars: HashMap<PackageId, FeaturesToVars>,
    input: &SolverInput,
    solution: &Solution,
    main_pkg: PackageId,
) -> QuackResult<SolverFreeze> {
    let all_pkgs = all_packages(package_vars, input, solution);
    let mut all_features = all_features(&package_to_feature_vars, &all_pkgs, input, solution);
    let mut all_realizations = all_realizations(&all_pkgs, &all_features, input)?;
    let mut pkg_freezes = HashMap::new();
    for pkg in all_pkgs {
        let features = all_features
            .remove(&pkg)
            .with_context_internal(|| format!("no features for {pkg:?}"))?;
        let realizations = all_realizations
            .remove(&pkg)
            .with_context_internal(|| format!("no realizations for {pkg:?}"))?;
        let pkg_freeze = SolverPackageFreeze {
            features,
            dependencies_realization: realizations,
        };
        pkg_freezes.insert(pkg, pkg_freeze);
    }
    Ok(SolverFreeze {
        package_freezes: pkg_freezes,
        main_pkg,
    })
}

/// Get all the packages that will be present in the [`SolverFreeze`].
/// Those are previous freeze packages and packages chosen in the [`Solution`].
fn all_packages(
    package_vars: HashMap<PackageId, Rc<Variable>>,
    input: &SolverInput,
    solution: &Solution,
) -> HashSet<PackageId> {
    let mut all_pkgs = input.all_preexisting_pkgs();
    let solution_pkgs = package_vars.into_iter().filter_map(|(pkg, var)| {
        if is_one(var.as_ref(), solution) {
            Some(pkg)
        } else {
            None
        }
    });
    all_pkgs.extend(solution_pkgs);
    trace!(?all_pkgs);
    all_pkgs
}

/// Get a mapping from all packages to all of their features that will be present in the [`SolverFreeze`].
/// Those are previous freeze features and features chosen in the [`Solution`].
fn all_features(
    package_to_feature_vars: &HashMap<PackageId, FeaturesToVars>,
    all_packages: &HashSet<PackageId>,
    input: &SolverInput,
    solution: &Solution,
) -> HashMap<PackageId, HashSet<FeatureName>> {
    let mut all_features = HashMap::new();
    for pkg in all_packages {
        let mut features = HashSet::new();
        if let Some(preexisting) = input.preexisting_features_for_pkg(*pkg) {
            features.extend(preexisting.iter().copied());
        }
        if let Some(feature_vars) = package_to_feature_vars.get(pkg) {
            let solution_features = feature_vars.iter().filter_map(|(feature, var)| {
                if is_one(var.as_ref(), solution) {
                    Some(feature)
                } else {
                    None
                }
            });
            features.extend(solution_features);
        }
        all_features.insert(*pkg, features);
    }
    trace!(?all_features);
    all_features
}

/// Get a mapping from all packages to realizations of their dependencies that will be present in the [`SolverFreeze`].
/// Note
/// ----
/// Here we heavily rely on the assumption that each package to be in the freeze
/// has a unique [`Idenitity`](crate::quackpack::core::package_identifiers::identity::Identity).
/// Thus we can choose a non-preexisting realization by looking at all the packages with the given
/// [`FullIdentity`](crate::quackpack::core::package_identifiers::full_identity::FullIdentity) and
/// take the unique one present in the solution.
fn all_realizations(
    all_packages: &HashSet<PackageId>,
    all_features: &HashMap<PackageId, HashSet<FeatureName>>,
    input: &SolverInput,
) -> QuackResult<HashMap<PackageId, HashMap<StrId, PackageId>>> {
    let mut all_realizations = HashMap::new();
    for pkg in all_packages {
        let pkg_data = input.package_data(*pkg)?;
        let features = all_features
            .get(pkg)
            .with_context_internal(|| format!("no features for {pkg:?}"))?;
        let mut pkg_realizations = HashMap::new();
        for dep in pkg_data
            .manifest()
            .dependencies()
            .select(&Selector::EnabledBy(features))
        {
            if let Some(realization) = pkg_data.dependency_preexists(dep.effective_name()) {
                pkg_realizations.insert(dep.effective_name(), realization);
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
                if all_packages.contains(&realization) {
                    pkg_realizations.insert(dep.effective_name(), realization);
                    break;
                }
            }
        }
        all_realizations.insert(*pkg, pkg_realizations);
    }
    trace!(?all_realizations);
    Ok(all_realizations)
}

/// Helper function for determining whether the model did or did not put the variable into the solution.
fn is_one(var: &Variable, solution: &Solution) -> bool {
    // The condition "> 0.5" is arbitrary, ideally it could be "== 1.0", but maybe to circumvent some float magic "> 0.5" is better.
    solution.val(var) > 0.5
}
