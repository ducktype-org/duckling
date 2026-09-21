//! This module contains a [`DependencyGraphFinder`] and [`SolverEngineData`] structs, which are designated
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
pub mod dependency_edge;
pub mod gathering;
pub mod git_access;
pub mod solver_freeze;
pub mod solver_mode;
pub mod solving;
pub mod util;

#[cfg(test)]
mod tests;

use core::fmt;
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;

use tracing::{debug, info, warn};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::gathering::gatherer::Gatherer;
use crate::quackpack::core::solver::gathering::gatherer_state::GatheredInfo;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::solver::solving::NoSolutionError;
use crate::quackpack::core::solver::solving::input::SolverInput;
use crate::quackpack::core::solver::solving::solver_engine::SolverEngine;
use crate::quackpack::core::storage::freeze::VenvFreeze;
use crate::quackpack::core::{FeatureName, Manifest, PackageContext, PackageId};
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal};

/// Main entry point.
/// Performs the whole solving process.
pub struct Solver<'duck, 'ctx, Access: GitAccess> {
    root_pcx: &'ctx PackageContext<'duck>,
    previous_freeze: Option<&'ctx VenvFreeze>,
    fetcher: &'ctx Fetcher<'duck>,
    git_access: Access,
    mode: SolverMode,
}

/// Dependency realization returned by the solving process.
/// Contains the new, currently valid [`SolverFreeze`] and its packages' manifests.
pub struct SolverAnswer {
    pub new_freeze: SolverFreeze,
    pub pkgs_manifests: HashMap<PackageId, Box<Manifest>>,
}

impl<'duck, 'ctx, Access: GitAccess> Solver<'duck, 'ctx, Access> {
    /// Create a new [`Solver`].
    pub fn new(
        root_pcx: &'ctx PackageContext<'duck>,
        previous_freeze: Option<&'ctx VenvFreeze>,
        fetcher: &'ctx Fetcher<'duck>,
        git_access: Access,
        mode: SolverMode
    ) -> Self {
        Self {
            root_pcx,
            previous_freeze,
            fetcher,
            git_access,
            mode
        }
    }

    /// Perform the whole solving process.
    pub async fn solve(self) -> QuackResult<SolverAnswer> {
        self.root_pcx.ctx().info("starting solving the dependency graph")?;
        let root_pkg = self.root_pcx.package_id()?;

        let (initial_freeze, empty_input) = self.get_initial_freeze(root_pkg)?;
        let gatherer = DependencyGraphFinder::new(self.root_pcx, initial_freeze, self.mode)
            .context("failed to start gathering packages")?;

        let fetcher_lock = self.root_pcx.ctx().duck_home().open_fetcher_lockfile(self.root_pcx.ctx())?;
        let should_run_engine = gatherer.prepare_solving(self.fetcher, &self.git_access).await?;
        drop(fetcher_lock);

        let solver_engine = match should_run_engine {
            ShouldRunSolverEngine::No(answer) => { return Ok(answer); },
            ShouldRunSolverEngine::Yes(solver_engine) => solver_engine,
        };
        match solver_engine.solve(self.root_pcx.ctx()) {
            Ok(answer) => Ok(answer),
            Err(e) => self.rerun_if_nonempty_input(e, empty_input).await,
        }
    }

    /// Create the initial [`SolverFreeze`], which serves as an entry point to the solving process.
    /// It is used as apriori knowledge and is modified by later steps.
    fn get_initial_freeze(&self, root_pkg: PackageId) -> QuackResult<(SolverFreeze, bool)> {
        match self.previous_freeze {
            Some(freeze) => match SolverFreeze::try_from_venv_freeze(root_pkg, freeze) {
                Ok(freeze) => Ok((freeze, false)),
                Err(malformed) => {
                    warn!(error = ?malformed, "previous storage freeze is malformed");
                    self.root_pcx.ctx()
                        .warning("previous freezefile was malformed, ignoring it")?;
                    Ok((SolverFreeze::empty_with_root(root_pkg)?, true))
                }
            },
            None => Ok((SolverFreeze::empty_with_root(root_pkg)?, true)),
        }
    }

    /// If there was a (well-formed) previous freeze, and we got [`NoSolutionError`],
    /// then it might be because because of the assumtpion that previous freeze packages must be present.
    /// Thus we retry with no input freeze, hoping that it allows for an existence of a solution.
    async fn rerun_if_nonempty_input(mut self, e: QuackError, empty_input: bool) -> QuackResult<SolverAnswer> {
        if !e.has_in_chain::<NoSolutionError>() || empty_input {
            return Err(e);
        }
        self.root_pcx.ctx().info("no solution found, retrying without previous freeze")?;
        self.remove_previous_freeze();
        self.solve().await
    }

    /// Helper for [`Self::rerun_if_nonempty_input`].
    /// Changes previous freeze to [`None`].
    fn remove_previous_freeze(&mut self) {
        self.previous_freeze = None;
    }
}

/// A struct designated to finding the full dependency graph of a given package.
pub struct DependencyGraphFinder<'duck, 'ctx> {
    root_pcx: &'ctx PackageContext<'duck>,
    root_pkg: PackageId,
    root_pkg_features: HashSet<FeatureName>,
    current_freeze: SolverFreeze,
    mode: SolverMode,
}

/// [`prepare_solving`](DependencyGraphFinder::prepare_solving) response describing whether we should
/// run the rest of the solver engine.
pub enum ShouldRunSolverEngine {
    No(SolverAnswer),
    Yes(Box<SolverEngineData>),
}

impl fmt::Display for ShouldRunSolverEngine {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let text = match self {
            Self::No(..) => "no",
            Self::Yes(..) => "yes",
        };
        f.write_str(text)
    }
}

impl<'duck, 'ctx> DependencyGraphFinder<'duck, 'ctx> {
    /// Creates a new [`DependencyGraphFinder`] instance.
    pub fn new(
        pcx: &'ctx PackageContext<'duck>,
        current_freeze: SolverFreeze,
        mode: SolverMode,
    ) -> QuackResult<Self> {
        let root_origin = FullOrigin::for_local(pcx.package().root())?;
        let root_identity = FullIdentity::new(pcx.package().name(), root_origin);
        Ok(Self {
            root_pcx: pcx,
            root_pkg: PackageId::new(root_identity, pcx.package().version()),
            root_pkg_features: pcx
                .package()
                .manifest()
                .features()
                .all_features()
                .keys()
                .copied()
                .collect(),
            current_freeze,
            mode,
        })
    }

    /// Determines if all the direct and transitive dependencies of the root package are satisfied.
    /// If not, prepares the [`SolverEngineData`] for running the engine by constructing [`SolverInput`].
    #[tracing::instrument(skip_all)]
    pub async fn prepare_solving<Access: GitAccess>(
        self,
        fetcher: &Fetcher<'_>,
        git_access: &Access,
    ) -> QuackResult<ShouldRunSolverEngine> {
        let ctx = fetcher.ctx();
        ctx.info("starting gathering the dependency graph")?;
        let gatherer = Gatherer::new(fetcher, git_access);

        let root_manifest = Box::new(self.root_pcx.package().manifest().clone());
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
        prev_freeze_manifests.insert(self.root_pkg, root_manifest.clone());
        let (maximal_valid_freeze, is_root_satisfied) = self
            .current_freeze
            .find_maximal_correct_dep_solution(&prev_freeze_manifests, fetcher)
            .await?;

        if is_root_satisfied {
            debug!("root has been satisfied");
            let trimmed = maximal_valid_freeze
                .find_minimal_dep_solution(&prev_freeze_manifests, root_features)?;
            ctx.info("no need to run the gathering or the solver engine")?;
            return Ok(ShouldRunSolverEngine::No(SolverAnswer {
                new_freeze: trimmed,
                pkgs_manifests: prev_freeze_manifests,
            }));
        } else if self.mode.frozen {
            qp_bail!(
                "Solver activated with the `--frozen` option but the main package dependencies were not satisfied inside the found freeze"
            )
        }

        let root_path = self.root_pcx.package().root().into();
        ctx.info("running gathering")?;
        let gathered_info = Self::run_solver_gatherer(
            &gatherer,
            root_manifest,
            root_path,
            root_features,
            &maximal_valid_freeze,
            self.mode,
        )
        .await?;
        info!(?gathered_info, "finished gathering");
        let solver_input = SolverInput::from_freeze_and_gathered_info(
            &maximal_valid_freeze,
            prev_freeze_manifests,
            gathered_info,
        )?;
        Ok(ShouldRunSolverEngine::Yes(Box::new(SolverEngineData {
            root_pkg: self.root_pkg,
            root_pkg_features: self.root_pkg_features,
            current_freeze: maximal_valid_freeze,
            input: solver_input,
        })))
    }

    /// Helper for [`Self::prepare_solving`].
    /// Runs the [`Gatherer`], to fetch all potentially necessary manifests.
    #[tracing::instrument(skip_all)]
    async fn run_solver_gatherer<Access: GitAccess>(
        gatherer: &Gatherer<'_, '_, Access>,
        mut root_manifest: Box<Manifest>,
        root_path: PathBuf,
        root_features: HashSet<FeatureName>,
        freeze: &SolverFreeze,
        mode: SolverMode,
    ) -> QuackResult<GatheredInfo> {
        Self::prepare_root_manifest_for_gathering(&mut root_manifest, freeze)?;
        gatherer
            .explore(root_path, root_manifest, root_features, mode)
            .await
    }

    /// Helper for [`Self::run_solver_gatherer`].
    /// Retains only unsatisfied dependencies in the root package's manifest, so that it can be used in gathering.
    fn prepare_root_manifest_for_gathering(
        root_manifest: &mut Manifest,
        freeze: &SolverFreeze,
    ) -> QuackResult<()> {
        let Some(root_freeze) = freeze.package_freezes.get(&freeze.main_pkg) else {
            qp_bail_internal!("maximal valid freeze without main package freeze {freeze:#?}")
        };
        root_manifest
            .dependencies_mut()
            .all_dependencies_mut()
            .retain(|dep| {
                !root_freeze
                    .dependencies_realization
                    .contains_key(&dep.effective_name())
            });
        Ok(())
    }
}

#[derive(Debug)]
/// A struct designated to finding the dependency resolution of a given package.
///
/// It can be created by [`prepare_solving`](DependencyGraphFinder::prepare_solving).
pub struct SolverEngineData {
    input: SolverInput,
    root_pkg: PackageId,
    root_pkg_features: HashSet<FeatureName>,
    current_freeze: SolverFreeze,
}

impl SolverEngineData {
    /// Finds dependency resolution of a given package.
    /// Returns a [`SolverAnswer`].
    #[tracing::instrument(skip_all)]
    pub fn solve(self, ctx: &DuckContext) -> QuackResult<SolverAnswer> {
        ctx.info("starting the solver engine")?;
        let (solution, manifests) =
            SolverEngine::run_engine(self.input, &(self.root_pkg, self.root_pkg_features))?;
        debug!(?solution);
        let new_freeze = self.current_freeze.new_freeze(&manifests, solution)?;
        Ok(SolverAnswer {
            new_freeze,
            pkgs_manifests: manifests,
        })
    }
}
