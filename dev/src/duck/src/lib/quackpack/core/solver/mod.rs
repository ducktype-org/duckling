//! This module contains a [`Solver`] struct, which is designated to finding the dependency resolution
//! of a given package.
//! By *dependency resolution* we mean a set of packages, each with a designated set of features,
//! so that each package's dependencies are satisfied inside that set.
//! 
//! How it works:
//! ---------------
//! The process of finding the resolution consists of the following steps:
//! 1. Getting the last known freeze (a representation of the dependency resolution) of the package
//!     (we want to reuse it as much as possible).
//! 2. Fetching manifests of the packages mentioned there (to see if their dependencies are still satisfied inside the freeze).
//! 3. Finding the maximal subset of the previous freeze which is still a proper dependency resolution
//!     (though we allow for the dependencies of the root package to not be satisfied).
//! 4. Running the gathering process ([`Gatherer`]) on the unsatisfied dependencies of the root package.
//! 5. Running the solver engine, which translates the problem into an instance of Integer Linear Programming and solves it with a 3-rd party solver.
//! 6. Creating the new freeze, based on the reused part of the previous freeze and the solver-engine output.
//! 7. Trimming the new freeze, to remove dependencies of the root package which were present previously
//!     but have been since removed from its manifest.
pub mod gathering;
pub mod git_access;
pub mod solver_freeze;
pub mod solving;
pub mod types_common;
pub mod util;

use std::{cell::OnceCell, marker::PhantomData, sync::Arc};

use tokio::sync::Mutex;

use crate::{
    QpCtx, QuackResult, qp_bail_internal,
    quackpack::core::{
        PackageCtx, fetcher::Fetcher, gathering::gatherer::Gatherer, git_access::GitAccess,
        solver_freeze::SolverFreeze, solving::solver_engine::SolverInput,
    },
};

pub enum SolverMode {
    Merciful,
    Strict,
}

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
    current_freeze: SolverFreeze,
    gathered_info: OnceCell<SolverInput>,
    state: PhantomData<State>,
}

impl<'duck> Solver<'duck, Prepared> {
    /// Creates a new [`Solver`] instance.
    pub fn new(
        package_ctx: &'duck PackageCtx<'duck>,
        fetcher: &'duck Fetcher<'duck>,
        current_freeze: SolverFreeze,
    ) -> Self {
        Self {
            qp_ctx: package_ctx.ctx(),
            root_package_ctx: package_ctx,
            fetcher,
            current_freeze,
            gathered_info: OnceCell::new(),
            state: PhantomData,
        }
    }

    /// Prepares the [`Solver`] for running the engine by constructing [`SolverInput`].
    pub async fn prepare_solving<Access: GitAccess>(
        mut self,
        git_access: Access,
    ) -> QuackResult<Solver<'duck, Prepared>> {
        let access = Arc::new(Mutex::new(git_access));
        let gatherer = Gatherer::new(self.qp_ctx, self.fetcher, access);

        let mut root_manifest = self.root_package_ctx.package().manifest().clone();
        let root_package = self.current_freeze.main_pkg;
        let mut prev_freeze_manifests = self
            .current_freeze
            .get_prev_freeze_manifests(&gatherer)
            .await?;
        prev_freeze_manifests.insert(root_package, Box::new(root_manifest.clone()));
        let maximal_valid_freeze = self
            .current_freeze
            .find_maximal_correct_dep_solution(&prev_freeze_manifests)?;
        let Some(root_freeze) = maximal_valid_freeze
            .package_freezes
            .get(&maximal_valid_freeze.main_pkg)
        else {
            qp_bail_internal!("Maximal valid freeze without main package freeze")
        };
        for dep in root_freeze.dependencies_realization.keys() {
            root_manifest
                .dependencies_mut()
                .all_dependencies_mut()
                .remove(dep);
        }

        let root_path = self.root_package_ctx.package().manifest_path().into();
        let root_features = root_manifest
            .features()
            .all_features()
            .keys()
            .copied()
            .collect();
        let gathered_info = gatherer
            .explore(root_path, root_manifest, root_features, SolverMode::Strict)
            .await?;
        let solver_input = SolverInput::from_freeze_and_gathered_info(
            &maximal_valid_freeze,
            prev_freeze_manifests,
            gathered_info,
        );
        self.current_freeze = maximal_valid_freeze;
        _ = self.gathered_info.set(solver_input);
        Ok(Solver {
            qp_ctx: self.qp_ctx,
            fetcher: self.fetcher,
            root_package_ctx: self.root_package_ctx,
            current_freeze: self.current_freeze,
            gathered_info: self.gathered_info,
            state: PhantomData,
        })
    }
}

impl<'duck> Solver<'duck, Prepared> {
    pub fn solve(self) -> QuackResult<SolverFreeze> {
        unimplemented!(
            "\
1. Solve the problem \
2. Generate a new freezefile"
        )
    }
}
