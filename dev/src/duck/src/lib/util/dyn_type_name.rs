// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! A trait (analog of [`Any`]) to get [`type_name`] from a `dyn T`.
//!
//! [`Any`]: std::any::Any

use std::any::type_name;

/// Get a [`type_name`] at runtime.
pub trait DynTypeName {
    fn type_name(&self) -> &'static str;
}

impl<T: ?Sized> DynTypeName for T {
    fn type_name(&self) -> &'static str {
        type_name::<T>()
    }
}
