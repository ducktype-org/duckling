// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! QuackPack, an engine behind Duckling's package management.
pub mod core;
pub mod schemas;
pub mod subcommands;
pub mod util;

// Re-exported, so QuackPack code and its users share the one error/result/context of the duck tools.
pub use duck_utils::util::str_id::*;
pub use duck_utils::{
    DuckContext, QuackError, QuackResult, QuackResultContext, qp_bail, qp_bail_internal, qp_err,
    qp_internal,
};
