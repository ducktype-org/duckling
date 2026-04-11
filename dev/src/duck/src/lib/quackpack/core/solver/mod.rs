//! This module contains a [`SolverGathererData`] and [`SolverEngineData`] structs, which are designated
//! to finding the dependency resolution of a given package.
//! By *dependency resolution* we mean a set of packages, each with a designated set of features,
//! so that each package's dependencies are satisfied inside that set.
//!
//! How it works:
//! ---------------
//! The process of finding the resolution consists of the following steps:
//! 1. Getting the last known freeze (a representation of the dependency resolution) of the package
//!    (we want to reuse it as much as possible).
//! 2. Fetching manifests of the packages mentioned there (to see if their dependencies are still satisfied inside the freeze).
//! 3. Finding the maximal subset of the previous freeze which is still a proper dependency resolution
//!    (though we allow for the dependencies of the root package to not be satisfied).
//! 4. Running the gathering process ([`Gatherer`]) on the unsatisfied dependencies of the root package.
//! 5. Running the solver engine, which translates the problem into an instance of Integer Linear Programming and solves it with a 3-rd party solver.
//! 6. Creating the new freeze, based on the reused part of the previous freeze and the solver-engine output.
//! 7. Trimming the new freeze, to remove dependencies of the root package which were present previously
//!    but have been since removed from its manifest.
pub mod gathering;
pub mod git_access;
pub mod solver_freeze;
pub mod solver_mode;
pub mod solving;
pub mod types_common;
pub mod util;

#[cfg(test)]
mod tests;

use std::{
    collections::{HashMap, HashSet},
    path::PathBuf,
};

use crate::{
    QuackResult, qp_bail, qp_bail_internal,
    quackpack::core::{
        FeatureName, Manifest, PackageCtx,
        fetcher::Fetcher,
        gathering::{gatherer::Gatherer, gatherer_state::GatheredInfo},
        git_access::GitAccess,
        solver_freeze::SolverFreeze,
        solver_mode::SolverMode,
        solving::solver_engine::{SolverEngine, SolverInput},
        types_common::{ExpandedLocation, ExpandedPackage, InternedExpandedLocation},
    },
};

/// A struct designated to finding the full dependency graph of a given package.
pub struct SolverGathererData<'duck, 'ctx> {
    root_package_ctx: &'ctx PackageCtx<'duck>,
    root_pkg: ExpandedPackage,
    root_pkg_features: HashSet<FeatureName>,
    current_freeze: SolverFreeze,
    mode: SolverMode,
}

/// Dependency realization returned by [`solve`](SolverEngineData::solve).
/// Contains the new, currently valid [`SolverFreeze`]
/// and its packages manifests to generate a serializable freeze.
pub struct SolverAnswer {
    pub new_freeze: SolverFreeze,
    pub pkgs_manifests: HashMap<ExpandedPackage, Box<Manifest>>,
}

/// [`prepare_solving`](SolverGathererData::prepare_solving) response describing whether we should
/// run the rest of the solver engine.
pub enum ShouldRunSolverEngine {
    No(SolverAnswer),
    Yes(Box<SolverEngineData>),
}

impl<'duck, 'ctx> SolverGathererData<'duck, 'ctx> {
    /// Creates a new [`SolverGathererData`] instance.
    pub fn new(
        package_ctx: &'ctx PackageCtx<'duck>,
        current_freeze: SolverFreeze,
        mode: SolverMode,
    ) -> Self {
        Self {
            root_package_ctx: package_ctx,
            root_pkg: ExpandedPackage {
                location: InternedExpandedLocation::new(ExpandedLocation::Local {
                    absolute_path: package_ctx.package().root_directory().to_path_buf(),
                }),
                version: None,
            },
            root_pkg_features: package_ctx
                .package()
                .manifest()
                .features()
                .all_features()
                .keys()
                .copied()
                .collect(),
            current_freeze,
            mode,
        }
    }

    /// Determines if all the transitive dependencies of the root package are satisfied.
    /// If not, prepares the [`SolverEngineData`] for running the engine by constructing [`SolverInput`].
    pub fn prepare_solving<Access: GitAccess>(
        self,
        fetcher: &mut Fetcher<'_>,
        git_access: &mut Access,
    ) -> QuackResult<ShouldRunSolverEngine> {
        let mut gatherer = Gatherer::new(fetcher, git_access);

        let root_manifest = self.root_package_ctx.package().manifest().clone();
        let root_features = root_manifest
            .features()
            .all_features()
            .keys()
            .copied()
            .collect();
        let mut prev_freeze_manifests = self
            .current_freeze
            .get_prev_freeze_manifests(&mut gatherer)?;
        prev_freeze_manifests.insert(self.root_pkg, Box::new(root_manifest.clone()));
        let (maximal_valid_freeze, is_root_satisfied) = self
            .current_freeze
            .find_maximal_correct_dep_solution(&prev_freeze_manifests)?;

        if is_root_satisfied {
            let trimmed = maximal_valid_freeze
                .find_minimal_dep_solution(&prev_freeze_manifests, root_features)?;
            return Ok(ShouldRunSolverEngine::No(SolverAnswer {
                new_freeze: trimmed,
                pkgs_manifests: prev_freeze_manifests,
            }));
        } else if self.mode.frozen {
            qp_bail!(
                "Solver activated with the `--frozen` option but the main package dependencies were not satisfied inside the found freeze"
            )
        }

        let root_path = self.root_package_ctx.package().root_directory().into();
        let gathered_info = Self::run_solver_gatherer(
            &mut gatherer,
            root_manifest,
            root_path,
            root_features,
            &maximal_valid_freeze,
            self.mode,
        )?;
        let solver_input = SolverInput::from_freeze_and_gathered_info(
            &maximal_valid_freeze,
            prev_freeze_manifests,
            gathered_info,
        );
        Ok(ShouldRunSolverEngine::Yes(Box::new(SolverEngineData {
            root_pkg: self.root_pkg,
            root_pkg_features: self.root_pkg_features,
            current_freeze: maximal_valid_freeze,
            input: solver_input,
        })))
    }

    /// Helper for [`Self::prepare_solving`].
    /// Runs the [`Gatherer`], to fetch all potentially necessary manifests.
    fn run_solver_gatherer<Access: GitAccess>(
        gatherer: &mut Gatherer<'_, '_, '_, Access>,
        root_manifest: Manifest,
        root_path: PathBuf,
        root_features: HashSet<FeatureName>,
        freeze: &SolverFreeze,
        mode: SolverMode,
    ) -> QuackResult<GatheredInfo> {
        let root_manifest_for_gathering =
            Self::prepare_root_manifest_for_gathering(root_manifest, freeze)?;
        gatherer.explore(root_path, root_manifest_for_gathering, root_features, mode)
    }

    /// Helper for [`Self::run_solver_gatherer`].
    /// Retains only unsatisfied dependencies in the root package's manifest, so that it can be used in gathering.
    fn prepare_root_manifest_for_gathering(
        mut root_manifest: Manifest,
        freeze: &SolverFreeze,
    ) -> QuackResult<Manifest> {
        let Some(root_freeze) = freeze.package_freezes.get(&freeze.main_pkg) else {
            qp_bail_internal!("Maximal valid freeze without main package freeze")
        };
        root_manifest
            .dependencies_mut()
            .all_dependencies_mut()
            .retain(|dep| {
                !root_freeze
                    .dependencies_realization
                    .contains_key(&dep.effective_name())
            });
        Ok(root_manifest)
    }
}

#[derive(Debug)]
/// A struct designated to finding the dependency resolution of a given package.
///
/// It can be created by [`prepare_solving`](SolverGathererData::prepare_solving).
pub struct SolverEngineData {
    input: SolverInput,
    root_pkg: ExpandedPackage,
    root_pkg_features: HashSet<FeatureName>,
    current_freeze: SolverFreeze,
}

impl SolverEngineData {
    /// Finds dependency resolution of a given package.
    /// Returns a [`SolverAnswer`].
    pub fn solve(self) -> QuackResult<SolverAnswer> {
        let manifests = self.input.gathered_manifests.clone();
        let solver_output =
            SolverEngine::run_engine(self.input, &(self.root_pkg, self.root_pkg_features))?;
        let new_freeze = self.current_freeze.new_freeze(&manifests, solver_output)?;
        Ok(SolverAnswer {
            new_freeze,
            pkgs_manifests: manifests,
        })
    }
}
