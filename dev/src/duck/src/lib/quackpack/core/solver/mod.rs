// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! This module contains a [`Solver`] struct which is designated to finding the dependency resolution of a given package.
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
//! 4. Running the gathering process ([`Gatherer`](gathering::gatherer::Gatherer)) on the unsatisfied dependencies of the root package.
//! 5. Running the solver engine, which translates the problem into an instance of Integer Linear Programming and solves it with a 3-rd party solver.
//! 6. Creating the new freeze, based on the reused part of the previous freeze and the solver-engine output.
//! 7. Trimming the new freeze, to remove dependencies of the root package which were present previously
//!    but have been since removed from its manifest.
//!
//! Reusing previous freeze:
//! ------------------------
//! Packages from the previous freeze remaining after stage 3 are assumed to be part of the solution until stage 7.
//! This sometimes can cause stage 5 to fail, because we prohibit same packages but in different versions from entering the solution.
//! To circumvent this, if [`NoSolutionError`](solving::NoSolutionError) is encountered, and the last known freeze was well-formed,
//! we repeat the whole process, but without supplying the last known freeze.
//!
//! Code layout:
//! ------------
//! - [`pipeline`] — the [`Solver`] itself: runs the steps above in order and handles the retry,
//! - [`root`] — [`SolverRoot`](root::SolverRoot), what the solver needs to know about the root package,
//! - [`solver_freeze`] — steps 2, 3, 6 and 7 (freeze diagnosis, new freeze generation and trimming),
//! - [`gathering`] — step 4,
//! - [`solving`] — step 5.
//!
//! Helpers shared by the steps:
//! - [`dependency_edge`] — [`DependencyEdge`](dependency_edge::DependencyEdge), one dependency of a package on a package identity,
//! - [`git_access`] — the [`GitAccess`](git_access::GitAccess) trait, the solver's view of where git packages are stored,
//! - [`solver_mode`] — [`SolverMode`](solver_mode::SolverMode), the solver options (e.g. `--frozen`),
//! - [`util`] — matching manifest dependencies against known package versions and building manifest requests.
pub mod dependency_edge;
pub mod gathering;
pub mod git_access;
pub mod pipeline;
mod root;
pub mod solver_freeze;
pub mod solver_mode;
pub mod solving;
pub mod util;

#[cfg(test)]
mod tests;

pub use pipeline::{Solver, SolverAnswer};
