use std::cell::OnceCell;

use crate::{QpCtx, QuackResult, quackpack::core::PackageCtx};

pub enum Todo {}
pub type VenvFreeze = Todo;
pub type GitAccess = Todo;

pub enum SolverState {
    Created,
    Prepared,
}

// This is so ugly, but that is completely Rust's fault, there are no generics by custom types.
pub const fn to_i32(state: SolverState) -> i32 {
    match state {
        SolverState::Created => 0,
        SolverState::Prepared => 1,
    }
}

pub struct Solver<'duck, const STATE: i32> {
    _qp_ctx: &'duck QpCtx<'duck>,
    _root_package_ctx: &'duck PackageCtx<'duck>,
    // Passing this needs further consideration from the storage:
    //  * package root is the freeze's primary location
    //  * in the case of its absence the freeze from the storage should be passed
    //  * in the case of its absence an empty freeze should be passed.
    _current_freeze: VenvFreeze,
    _gathered_info: OnceCell<Todo>,
}

impl<'duck> Solver<'duck, { to_i32(SolverState::Created) }> {
    pub fn _new(package_ctx: &'duck PackageCtx<'duck>, current_freeze: VenvFreeze) -> Self {
        Self {
            _qp_ctx: package_ctx.ctx(),
            _root_package_ctx: package_ctx,
            _current_freeze: current_freeze,
            _gathered_info: OnceCell::new(),
        }
    }

    pub async fn _prepare_solving(
        self,
        _git_access: &mut GitAccess,
    ) -> QuackResult<Solver<'duck, { to_i32(SolverState::Prepared) }>> {
        todo!(
            "\
1. Determine which dependencies are unsatisfied,
    this requires also checking all local dependencies freezefiles.
2. Gather manifests of packages possibly appearing in the solution.
            "
        );
    }
}

impl<'duck> Solver<'duck, { to_i32(SolverState::Prepared) }> {
    pub fn _solve(self) -> QuackResult<VenvFreeze> {
        todo!(
            "\
1. Solve the problem \
2. Generate a new freezefile"
        )
    }
}
