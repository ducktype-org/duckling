// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Extract a value.

use std::sync::LockResult;

use tracing::warn;

pub trait Extract<T> {
    /// Extract a value.
    fn extract(self) -> T;
}

impl<T> Extract<T> for LockResult<T> {
    fn extract(self) -> T {
        match self {
            Self::Ok(value) => value,
            Self::Err(poison) => {
                warn!(error = %poison, "poisoned");
                poison.into_inner()
            }
        }
    }
}
