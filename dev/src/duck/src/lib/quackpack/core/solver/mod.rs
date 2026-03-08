//! This module contains a [`Solver`] struct, which is designated to finding the dependency resolution
//! of a given package.
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
    marker::PhantomData,
    path::PathBuf,
    sync::Arc,
};

use tokio::sync::Mutex;

use crate::{
    QpCtx, QuackResult, qp_bail, qp_bail_internal,
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

pub trait SolverState {}

pub struct Created;
pub struct Prepared;

impl SolverState for Created {}
impl SolverState for Prepared {}

/// A struct designated to finding the dependency resolution of a given package.
pub struct Solver<'duck, State: SolverState> {
    qp_ctx: &'duck QpCtx<'duck>,
    fetcher: &'duck Fetcher<'duck>,
    root_package_ctx: &'duck PackageCtx<'duck>,
    root_pkg: ExpandedPackage,
    root_pkg_features: HashSet<FeatureName>,
    current_freeze: SolverFreeze,
    gathered_info: Option<SolverInput>,
    state: PhantomData<State>,
    mode: SolverMode,
}

pub struct SolverAnswer {
    pub new_freeze: SolverFreeze,
    pub pkgs_manifests: HashMap<ExpandedPackage, Box<Manifest>>,
}

pub enum ShouldRunSolverEngine<'duck> {
    No(SolverAnswer),
    Yes(Box<Solver<'duck, Prepared>>),
}

impl<'duck> Solver<'duck, Prepared> {
    /// Creates a new [`Solver`] instance.
    pub fn new(
        package_ctx: &'duck PackageCtx<'duck>,
        fetcher: &'duck Fetcher<'duck>,
        current_freeze: SolverFreeze,
        mode: SolverMode,
    ) -> Self {
        Self {
            qp_ctx: package_ctx.ctx(),
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
            fetcher,
            current_freeze,
            gathered_info: None,
            state: PhantomData,
            mode,
        }
    }

    /// Determines if all the transitive dependencies of the root package are satisfied.
    /// If not, prepares the [`Solver`] for running the engine by constructing [`SolverInput`].
    pub async fn prepare_solving<GitAccessImpl: GitAccess>(
        self,
        git_access: Arc<Mutex<GitAccessImpl>>,
    ) -> QuackResult<ShouldRunSolverEngine<'duck>> {
        let gatherer = Gatherer::new(self.qp_ctx, self.fetcher, git_access);

        let root_manifest = self.root_package_ctx.package().manifest().clone();
        let root_features = root_manifest
            .features()
            .all_features()
            .keys()
            .copied()
            .collect();
        let mut prev_freeze_manifests = self
            .current_freeze
            .get_prev_freeze_manifests(&gatherer)
            .await?;
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
                "Solver activated with the --frozen option but the main package dependencies were not satisfied inside the found freeze"
            )
        }

        let root_path = self.root_package_ctx.package().root_directory().into();
        let gathered_info = Self::run_solver_gatherer(
            &gatherer,
            root_manifest,
            root_path,
            root_features,
            &maximal_valid_freeze,
            self.mode,
        )
        .await?;
        let solver_input = SolverInput::from_freeze_and_gathered_info(
            &maximal_valid_freeze,
            prev_freeze_manifests,
            gathered_info,
        );
        Ok(ShouldRunSolverEngine::Yes(Box::new(Solver {
            qp_ctx: self.qp_ctx,
            fetcher: self.fetcher,
            root_package_ctx: self.root_package_ctx,
            root_pkg: self.root_pkg,
            root_pkg_features: self.root_pkg_features,
            current_freeze: maximal_valid_freeze,
            gathered_info: Some(solver_input),
            state: PhantomData,
            mode: self.mode,
        })))
    }

    /// Helper for [`Self::prepare_solving`].
    /// Runs the [`Gatherer`], to fetch all potentially necessary manifests.
    async fn run_solver_gatherer<GitAccessImpl: GitAccess>(
        gatherer: &Gatherer<'duck, GitAccessImpl>,
        root_manifest: Manifest,
        root_path: PathBuf,
        root_features: HashSet<FeatureName>,
        freeze: &SolverFreeze,
        mode: SolverMode,
    ) -> QuackResult<GatheredInfo> {
        let root_manifest_for_gathering =
            Self::prepare_root_manifest_for_gathering(root_manifest, freeze)?;
        gatherer
            .explore(root_path, root_manifest_for_gathering, root_features, mode)
            .await
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
        for dep in root_freeze.dependencies_realization.keys() {
            root_manifest
                .dependencies_mut()
                .all_dependencies_mut()
                .remove(dep);
        }
        Ok(root_manifest)
    }
}

impl<'duck> Solver<'duck, Prepared> {
    pub fn solve(self) -> QuackResult<SolverAnswer> {
        let Some(input) = self.gathered_info else {
            qp_bail_internal!("Tried to run solver without input specified");
        };
        let manifests = input.gathered_manifests.clone();
        let solver_output =
            SolverEngine::run_engine(input, &(self.root_pkg, self.root_pkg_features))?;
        let new_freeze = self.current_freeze.new_freeze(&manifests, solver_output)?;
        Ok(SolverAnswer {
            new_freeze,
            pkgs_manifests: manifests,
        })
    }
}
