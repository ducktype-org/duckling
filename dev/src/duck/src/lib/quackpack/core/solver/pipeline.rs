// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! The [`Solver`] pipeline, gluing together the freeze diagnosis ([`solver_freeze`](super::solver_freeze)),
//! the [`Gatherer`] and the [`SolverEngine`]. The steps are described in the [module documentation](super).

use std::collections::HashMap;

use tracing::{debug, trace};

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::solver::gathering::gatherer::Gatherer;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::root::SolverRoot;
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::solver::solving::NoSolutionError;
use crate::quackpack::core::solver::solving::input::SolverInput;
use crate::quackpack::core::solver::solving::solver_engine::SolverEngine;
use crate::quackpack::core::storage::freeze::VenvFreeze;
use crate::quackpack::core::{Manifest, PackageContext, PackageId};
use crate::{QuackResult, qp_bail};

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
        mode: SolverMode,
    ) -> Self {
        Self {
            root_pcx,
            previous_freeze,
            fetcher,
            git_access,
            mode,
        }
    }

    /// Perform the whole solving process.
    ///
    /// The process starts from the previous freeze (step 1). If that attempt finds no solution,
    /// it is retried once from an empty freeze: the packages kept from the previous freeze
    /// may be what makes the problem infeasible.
    pub async fn solve(self) -> QuackResult<SolverAnswer> {
        let ctx = self.root_pcx.ctx();
        ctx.info_verbose("starting solving the dependency graph")?;
        let root = SolverRoot::new(self.root_pcx)?;

        let Some(previous_freeze) = self.previous_solver_freeze(&root)? else {
            return self
                .solve_from(&root, SolverFreeze::empty_with_root(root.id)?)
                .await;
        };
        match self.solve_from(&root, previous_freeze).await {
            Err(e) if e.has_in_chain::<NoSolutionError>() => {
                ctx.info_verbose("no solution found, retrying without previous freeze")?;
                self.solve_from(&root, SolverFreeze::empty_with_root(root.id)?)
                    .await
            }
            result => result,
        }
    }

    /// Step 1: converts the previous freeze into a [`SolverFreeze`].
    /// Returns [`None`] when there is no previous freeze or when it is malformed (with a warning).
    fn previous_solver_freeze(&self, root: &SolverRoot) -> QuackResult<Option<SolverFreeze>> {
        let Some(freeze) = self.previous_freeze else {
            return Ok(None);
        };
        match SolverFreeze::try_from_venv_freeze(root.id, freeze) {
            Ok(freeze) => Ok(Some(freeze)),
            Err(malformed) => {
                debug!(target: "duck::solver", error = ?malformed, "previous storage freeze is malformed");
                self.root_pcx
                    .ctx()
                    .warning("previous freezefile was malformed, ignoring it")?;
                Ok(None)
            }
        }
    }

    /// Steps 2-7, starting from `initial_freeze` as the a priori knowledge.
    ///
    /// The fetcher lock is held only while manifests are fetched (steps 2-4).
    #[tracing::instrument(target = "duck::solver", level = "debug", skip_all)]
    async fn solve_from(
        &self,
        root: &SolverRoot,
        initial_freeze: SolverFreeze,
    ) -> QuackResult<SolverAnswer> {
        let ctx = self.root_pcx.ctx();
        let fetcher_lock = ctx.duck_home().open_fetcher_lockfile(ctx)?;
        ctx.info_verbose("starting gathering the dependency graph")?;
        let gatherer = Gatherer::new(self.fetcher, &self.git_access);

        let mut manifests = initial_freeze.get_prev_freeze_manifests(&gatherer).await?;
        manifests.insert(root.id, root.manifest.clone());
        let (valid_freeze, is_root_satisfied) = initial_freeze
            .find_maximal_correct_dep_solution(root.features.clone(), &manifests, self.fetcher)
            .await?;
        if is_root_satisfied {
            debug!(target: "duck::solver", "the previous freeze still satisfies the root, trimming it");
            let new_freeze =
                valid_freeze.find_minimal_dep_solution(&manifests, root.features.clone())?;
            ctx.info_verbose("no need to run the gathering or the solver engine")?;
            return Ok(SolverAnswer {
                new_freeze,
                pkgs_manifests: manifests,
            });
        }
        if self.mode.frozen {
            qp_bail!(
                "Solver activated with the `--frozen` option but the main package dependencies were not satisfied inside the found freeze"
            )
        }

        let input = self
            .gather(root, &gatherer, &valid_freeze, manifests)
            .await?;
        drop(fetcher_lock);
        self.run_engine(root, valid_freeze, input)
    }

    /// Step 4: runs the [`Gatherer`] on the root dependencies not realized in `valid_freeze`,
    /// and merges what it found with the manifests of the freeze into a [`SolverInput`].
    async fn gather(
        &self,
        root: &SolverRoot,
        gatherer: &Gatherer<'_, '_, Access>,
        valid_freeze: &SolverFreeze,
        freeze_manifests: HashMap<PackageId, Box<Manifest>>,
    ) -> QuackResult<SolverInput> {
        self.root_pcx.ctx().info_verbose("running gathering")?;
        let gathered_info = gatherer
            .explore(
                root.path.clone(),
                root.manifest_for_gathering(valid_freeze)?,
                root.features.clone(),
                self.mode,
            )
            .await?;
        trace!(target: "duck::solver", ?gathered_info, "finished gathering");
        SolverInput::from_freeze_and_gathered_info(valid_freeze, freeze_manifests, gathered_info)
    }

    /// Steps 5-7: runs the [`SolverEngine`] and builds the new, trimmed freeze from its output.
    #[tracing::instrument(target = "duck::solver", level = "debug", skip_all)]
    fn run_engine(
        &self,
        root: &SolverRoot,
        valid_freeze: SolverFreeze,
        input: SolverInput,
    ) -> QuackResult<SolverAnswer> {
        self.root_pcx
            .ctx()
            .info_verbose("starting the solver engine")?;
        let (solution, manifests) =
            SolverEngine::run_engine(input, &(root.id, root.features.clone()))?;
        debug!(target: "duck::solver", ?solution);
        let new_freeze = valid_freeze.new_freeze(&manifests, solution)?;
        Ok(SolverAnswer {
            new_freeze,
            pkgs_manifests: manifests,
        })
    }
}
