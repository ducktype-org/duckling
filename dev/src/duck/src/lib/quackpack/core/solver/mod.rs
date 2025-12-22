use std::{cell::OnceCell, marker::PhantomData};

use crate::{QpCtx, QuackResult, quackpack::core::PackageCtx};

pub enum ToImplement {}
pub type VenvFreeze = ToImplement;
pub type GitAccess = ToImplement;

pub trait SolverState {}

pub struct Created();
pub struct Prepared();

impl SolverState for Created {}
impl SolverState for Prepared {}

pub struct Solver<'duck, State: SolverState> {
    _qp_ctx: &'duck QpCtx<'duck>,
    _root_package_ctx: &'duck PackageCtx<'duck>,
    // Passing this needs further consideration from the storage:
    //  * package root is the freeze's primary location
    //  * in the case of its absence the freeze from the storage should be passed
    //  * in the case of its absence an empty freeze should be passed.
    _current_freeze: VenvFreeze,
    _gathered_info: OnceCell<ToImplement>,
    _state: PhantomData<State>,
}

impl<'duck> Solver<'duck, Prepared> {
    pub fn new(package_ctx: &'duck PackageCtx<'duck>, current_freeze: VenvFreeze) -> Self {
        Self {
            _qp_ctx: package_ctx.ctx(),
            _root_package_ctx: package_ctx,
            _current_freeze: current_freeze,
            _gathered_info: OnceCell::new(),
            _state: PhantomData,
        }
    }

    pub async fn prepare_solving(
        self,
        _git_access: &mut GitAccess,
    ) -> QuackResult<Solver<'duck, Prepared>> {
        todo!(
            "\
1. Determine which dependencies are unsatisfied,
    this requires also checking all local dependencies freezefiles.
2. Gather manifests of packages possibly appearing in the solution.
            "
        );
    }
}

impl<'duck> Solver<'duck, Prepared> {
    pub fn _solve(self) -> QuackResult<VenvFreeze> {
        todo!(
            "\
1. Solve the problem \
2. Generate a new freezefile"
        )
    }
}
