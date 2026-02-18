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

pub struct Solver<'duck, State: SolverState> {
    qp_ctx: &'duck QpCtx<'duck>,
    fetcher: &'duck Fetcher<'duck>,
    root_package_ctx: &'duck PackageCtx<'duck>,
    // Passing this needs further consideration from the storage:
    //  * package root is the freeze's primary location
    //  * in the case of its absence the freeze from the storage should be passed
    //  * in the case of its absence an empty freeze should be passed.
    current_freeze: SolverFreeze,
    gathered_info: OnceCell<SolverInput>,
    state: PhantomData<State>,
}

impl<'duck> Solver<'duck, Prepared> {
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
