// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use crate::quackpack::core::storage::StorageSyncOptions;

#[derive(Debug, Clone, Copy)]
#[cfg_attr(test, derive(Default))]
/// A struct containing options for the solver.
/// The options are as follows:
/// * suppress_foreign_manifests_errors - whether errors in foreign manifests
///   (such as requiring a non-existent feature) should be propagated and end quackpack's
///   execution with an error or be suppressed,
/// * frozen - whether to return an error if the previous freeze is no longer valid.
pub struct SolverMode {
    pub suppress_foreign_manifests_errors: bool,
    pub frozen: bool,
}

impl From<StorageSyncOptions> for SolverMode {
    fn from(value: StorageSyncOptions) -> Self {
        Self {
            suppress_foreign_manifests_errors: value.strict_errors,
            frozen: value.frozen,
        }
    }
}
