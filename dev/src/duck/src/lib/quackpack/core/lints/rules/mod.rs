// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Implementation of lint passes.

use super::Lint;

pub mod aliases_equal_to_names;
pub mod always_false_conditions;
pub mod nonexistent_features;
pub mod self_implying_features;
pub mod unknown_lints;
mod util;

/// All known lints.
pub const ALL_LINTS: &[&Lint] = &[
    &aliases_equal_to_names::LINT,
    &always_false_conditions::LINT,
    &nonexistent_features::LINT,
    &self_implying_features::LINT,
    &unknown_lints::LINT,
];
