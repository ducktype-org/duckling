// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! [`OnceLock`] extension trait.

use std::sync::OnceLock;

pub trait OnceLockExt {
    type T;

    /// Try to init this cell with a fallible function.
    ///
    /// This function will fail if and only if `f` fails.
    fn try_init_with<E>(&self, f: impl FnOnce() -> Result<Self::T, E>) -> Result<&Self::T, E>;
}

impl<T> OnceLockExt for OnceLock<T> {
    type T = T;

    #[track_caller]
    fn try_init_with<E>(&self, f: impl FnOnce() -> Result<Self::T, E>) -> Result<&Self::T, E> {
        // Fast path for already initialized cells.
        if let Some(inner) = self.get() {
            return Ok(inner);
        }

        let value = f()?;

        // We can try to access this cell concurrently. Discard any errors.
        let _ = self.set(value);

        Ok(self.get().expect("called `set` above"))
    }
}
