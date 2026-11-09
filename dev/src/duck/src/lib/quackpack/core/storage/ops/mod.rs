// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Supported operations on a storage.
mod clean;
mod info;
mod sync;

pub use clean::*;
pub use info::*;
pub use sync::*;
