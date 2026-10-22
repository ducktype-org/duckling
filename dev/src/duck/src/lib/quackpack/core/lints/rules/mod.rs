// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Implementation of lint passes.

pub mod aliases_equal_to_names;
pub mod always_false_conditions;
pub mod nonexistent_features;
pub mod self_implying_features;
mod util;
