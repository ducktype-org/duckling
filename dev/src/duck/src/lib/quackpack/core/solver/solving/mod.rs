// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

pub mod input;
mod scip_ext;
pub mod solver_engine;
mod solver_model;

pub use solver_model::{FoundSolution, NoSolutionError};
